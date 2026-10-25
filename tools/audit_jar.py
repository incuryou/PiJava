#!/usr/bin/env python3
"""Audit a game's class-file dependencies against this VM's bundled classes.

This is a conservative static ABI audit, not a proof that a game runs. It checks
all game classes and the transitive constant-pool/descriptor dependencies of
bundled classes they reference, including unused methods. It never loads the
host JDK. Native bindings, abstract dispatch and method behavior remain untested.
"""
import argparse
from collections import deque
from dataclasses import dataclass
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
ACC_PUBLIC = 0x0001
ACC_STATIC = 0x0008
ACC_NATIVE = 0x0100
ACC_INTERFACE = 0x0200
ACC_ABSTRACT = 0x0400


class ClassError(ValueError):
    pass


class Reader:
    def __init__(self, data):
        self.data = data
        self.position = 0

    def take(self, count):
        end = self.position + count
        if end > len(self.data):
            raise ClassError('truncated class file')
        result = self.data[self.position:end]
        self.position = end
        return result

    def u1(self):
        return self.take(1)[0]

    def u2(self):
        return struct.unpack('>H', self.take(2))[0]

    def u4(self):
        return struct.unpack('>I', self.take(4))[0]


def modified_utf8(raw):
    # Class files encode NUL specially and supplementary characters as UTF-16
    # surrogate pairs, rather than standard four-byte UTF-8 sequences.
    try:
        text = raw.replace(b'\xc0\x80', b'\0').decode('utf-8', 'surrogatepass')
        return text.encode('utf-16-le', 'surrogatepass').decode('utf-16-le', 'surrogatepass')
    except UnicodeError as error:
        raise ClassError('invalid modified UTF-8 constant') from error


def descriptor_classes(descriptor):
    return set(re.findall(r'L([^;]+);', descriptor))


def class_components(name):
    # Primitive array types have no externally loadable component class.
    return descriptor_classes(name) if name.startswith('[') else {name}


@dataclass
class Member:
    name: str
    descriptor: str
    access: int
    code: bytes = None

    def behavior(self):
        if self.access & ACC_NATIVE:
            return 'native_binding_not_verified'
        if self.access & ACC_ABSTRACT:
            return 'abstract_dispatch_not_verified'
        if self.code == b'\xb1':
            return 'empty_void_body_not_verified'
        if self.code and len(self.code) == 2:
            first, last = self.code
            if ((first == 1 and last == 176)
                    or (2 <= first <= 8 and last == 172)
                    or (first in (9, 10) and last == 173)
                    or (11 <= first <= 13 and last == 174)
                    or (first in (14, 15) and last == 175)):
                return 'constant_return_body_not_verified'
        return 'bytecode_behavior_not_verified'


@dataclass
class ClassFile:
    name: str
    superclass: str
    interfaces: list
    access: int
    major: int
    methods: dict
    fields: dict
    class_references: set
    member_references: list
    source: str
    modern_constants: bool


def parse_class(data, source):
    reader = Reader(data)
    if reader.u4() != 0xcafebabe:
        raise ClassError('invalid class-file magic')
    reader.u2()  # minor version
    major = reader.u2()
    constant_pool = [None] * reader.u2()
    modern_constants = False
    index = 1
    while index < len(constant_pool):
        tag = reader.u1()
        if tag == 1:
            entry = (tag, modified_utf8(reader.take(reader.u2())))
        elif tag in (3, 4):
            entry = (tag, reader.take(4))
        elif tag in (5, 6):
            entry = (tag, reader.take(8))
        elif tag in (7, 8, 16, 19, 20):
            entry = (tag, reader.u2())
        elif tag in (9, 10, 11, 12, 17, 18):
            entry = (tag, reader.u2(), reader.u2())
        elif tag == 15:
            entry = (tag, reader.u1(), reader.u2())
        else:
            raise ClassError('unsupported constant-pool tag ' + str(tag))
        constant_pool[index] = entry
        if tag in (15, 16, 17, 18, 19, 20):
            modern_constants = True
        index += 2 if tag in (5, 6) else 1
        if index > len(constant_pool):
            raise ClassError('invalid wide constant-pool entry')

    def constant(index, expected):
        if index <= 0 or index >= len(constant_pool):
            raise ClassError('constant-pool index out of range')
        entry = constant_pool[index]
        if entry is None or entry[0] != expected:
            raise ClassError('unexpected constant-pool type')
        return entry

    def utf(index):
        return constant(index, 1)[1]

    def class_name(index):
        return utf(constant(index, 7)[1])

    def attributes():
        result = []
        for _ in range(reader.u2()):
            name = utf(reader.u2())
            result.append((name, reader.take(reader.u4())))
        return result

    access = reader.u2()
    name = class_name(reader.u2())
    super_index = reader.u2()
    superclass = class_name(super_index) if super_index else None
    interfaces = [class_name(reader.u2()) for _ in range(reader.u2())]

    def members(with_code):
        result = {}
        for _ in range(reader.u2()):
            flags = reader.u2()
            member_name = utf(reader.u2())
            descriptor = utf(reader.u2())
            code = None
            for attribute_name, payload in attributes():
                if with_code and attribute_name == 'Code':
                    code_reader = Reader(payload)
                    code_reader.u2()  # max_stack
                    code_reader.u2()  # max_locals
                    code = code_reader.take(code_reader.u4())
            key = (member_name, descriptor)
            if key in result:
                raise ClassError('duplicate member ' + member_name + descriptor)
            result[key] = Member(member_name, descriptor, flags, code)
        return result

    fields = members(False)
    methods = members(True)
    attributes()
    if reader.position != len(data):
        raise ClassError('trailing class-file bytes')
    references = set()
    member_references = []
    for entry in constant_pool:
        if entry is None:
            continue
        tag = entry[0]
        if tag == 7:
            references.update(class_components(utf(entry[1])))
        elif tag in (12, 16):
            references.update(descriptor_classes(utf(entry[2] if tag == 12 else entry[1])))
        elif tag in (9, 10, 11):
            owner = class_name(entry[1])
            name_and_type = constant(entry[2], 12)
            member_references.append((tag, owner, utf(name_and_type[1]), utf(name_and_type[2])))
    for member in list(fields.values()) + list(methods.values()):
        references.update(descriptor_classes(member.descriptor))
    return ClassFile(name, superclass, interfaces, access, major, methods, fields,
                     references, member_references, source, modern_constants)


class Auditor:
    def __init__(self, jar, classes):
        self.jar = Path(jar)
        self.classes = Path(classes)
        self.game = {}
        self.cache = {}
        self.parse_errors = []
        self.warnings = []
        self.missing = {}
        self.scanned = {}

    def load_jar(self):
        with zipfile.ZipFile(self.jar) as archive:
            for entry in sorted(archive.infolist(), key=lambda item: item.filename):
                if not entry.filename.endswith('.class'):
                    continue
                if entry.filename.startswith('META-INF/versions/'):
                    self.warnings.append('Ignored multi-release entry: ' + entry.filename)
                    continue
                source = str(self.jar) + '!' + entry.filename
                try:
                    parsed = parse_class(archive.read(entry), source)
                    if parsed.name in self.game:
                        raise ClassError('duplicate game class ' + parsed.name)
                    if entry.filename != parsed.name + '.class':
                        self.warnings.append('Class path/name mismatch: ' + source)
                    self.game[parsed.name] = parsed
                except (ClassError, ValueError) as error:
                    self.parse_errors.append(dict(source=source, error=str(error)))
        if not self.game:
            self.warnings.append('JAR contains no readable ordinary classes')

    def load(self, name):
        if name.startswith('['):
            return self.load('java/lang/Object')
        if name in self.cache:
            return self.cache[name]
        # Mirror VM load order: classpath/bundled classes precede game classes.
        if name.startswith('/') or any(part in ('', '.', '..') for part in name.split('/')):
            self.parse_errors.append(dict(source=name, error='invalid internal class name'))
            self.cache[name] = None
            return None
        path = self.classes / (name + '.class')
        parsed = None
        if path.is_file():
            try:
                parsed = parse_class(path.read_bytes(), str(path))
                if parsed.name != name:
                    raise ClassError('bundled class has mismatching internal name')
                if name in self.game:
                    self.warnings.append('Bundled class shadows JAR definition: ' + name)
            except (ClassError, ValueError) as error:
                self.parse_errors.append(dict(source=str(path), error=str(error)))
                parsed = None
        elif name in self.game:
            parsed = self.game[name]
        self.cache[name] = parsed
        return parsed

    def resolve_field(self, owner, key, seen=None):
        seen = set() if seen is None else seen
        if owner in seen:
            return None
        seen.add(owner)
        definition = self.load(owner)
        if definition is None:
            return None
        if key in definition.fields:
            return owner, definition.fields[key]
        # JVMS field lookup: declaration, superinterfaces, then superclass.
        for parent in definition.interfaces + ([definition.superclass] if definition.superclass else []):
            found = self.resolve_field(parent, key, seen)
            if found:
                return found
        return None

    def resolve_method(self, owner, key, interface_ref):
        if owner.startswith('['):
            if key == ('clone', '()Ljava/lang/Object;'):
                return owner, None
            owner = 'java/lang/Object'
        definition = self.load(owner)
        if definition is None:
            return None
        if bool(definition.access & ACC_INTERFACE) != interface_ref:
            return None
        if key[0] in ('<init>', '<clinit>'):
            member = definition.methods.get(key)
            return (owner, member) if member else None
        interfaces = []
        visited = set()
        current = definition
        while current and current.name not in visited:
            visited.add(current.name)
            member = current.methods.get(key)
            if member:
                return current.name, member
            interfaces.extend(current.interfaces)
            if interface_ref:
                break
            current = self.load(current.superclass) if current.superclass else None
        if interface_ref:
            object_class = self.load('java/lang/Object')
            member = object_class.methods.get(key) if object_class else None
            if member and member.access & ACC_PUBLIC and not member.access & ACC_STATIC:
                return 'java/lang/Object', member
        pending = deque(interfaces)
        while pending:
            name = pending.popleft()
            if name in visited:
                continue
            visited.add(name)
            parent = self.load(name)
            if parent is None:
                continue
            member = parent.methods.get(key)
            if member and not member.access & ACC_STATIC:
                return name, member
            pending.extend(parent.interfaces)
        return None

    def audit(self):
        self.load_jar()
        pending = deque(sorted(self.game))
        while pending:
            name = pending.popleft()
            if name in self.scanned:
                continue
            definition = self.load(name)
            if definition is None:
                continue
            self.scanned[name] = definition
            if definition.major > 52 or definition.modern_constants:
                self.warnings.append('Modern class-file features require separate VM support: ' + name)
            for reference in sorted(definition.class_references):
                if self.load(reference) is None:
                    self.missing.setdefault(reference, set()).add(name)
                elif reference not in self.scanned:
                    pending.append(reference)

        unresolved_methods = {}
        unresolved_fields = {}
        resolved_methods = {}
        for name, definition in sorted(self.scanned.items()):
            for tag, owner, member_name, descriptor in definition.member_references:
                key = (owner, member_name, descriptor, tag)
                if tag == 9:
                    found = self.resolve_field(owner, (member_name, descriptor))
                    if not found:
                        unresolved_fields.setdefault(key, set()).add(name)
                else:
                    found = self.resolve_method(owner, (member_name, descriptor), tag == 11)
                    if not found:
                        unresolved_methods.setdefault(key, set()).add(name)
                    else:
                        declared_owner, member = found
                        target = (declared_owner, member_name, descriptor)
                        if target not in resolved_methods:
                            declared_class = self.load(declared_owner) if member else None
                            resolved_methods[target] = dict(owner=declared_owner, name=member_name,
                                descriptor=descriptor,
                                behavior_status=member.behavior() if member else 'vm_array_clone_not_verified',
                                access_flags=member.access if member else None,
                                definition_source=declared_class.source if declared_class else 'VM array builtin',
                                code_length=len(member.code) if member and member.code is not None else None,
                                referenced_by=set())
                        resolved_methods[target]['referenced_by'].add(name)
        def unresolved(items):
            return [dict(owner=key[0], name=key[1], descriptor=key[2],
                         reference_kind={9: 'field', 10: 'method', 11: 'interface_method'}[key[3]],
                         referenced_by=sorted(referrers))
                    for key, referrers in sorted(items.items())]
        resolved = []
        for key in sorted(resolved_methods):
            item = resolved_methods[key]
            item['referenced_by'] = sorted(item['referenced_by'])
            resolved.append(item)
        native = [item for item in resolved if item['behavior_status'] == 'native_binding_not_verified']
        abstract = [item for item in resolved if item['behavior_status'] == 'abstract_dispatch_not_verified']
        trivial = [item for item in resolved if item['behavior_status'] in
                   ('empty_void_body_not_verified', 'constant_return_body_not_verified')]
        abi_complete = not (self.missing or unresolved_methods or unresolved_fields or self.parse_errors)
        return dict(schema_version=1, jar=str(self.jar), bundled_classes=str(self.classes),
            scope='all game classes plus conservative transitive class-file dependencies',
            abi_complete=abi_complete, behavior_verified=False,
            limitations=[
                'Symbol resolution is static and conservative; unused code can produce findings.',
                'Native declaration presence does not verify a C++ binding or its behavior.',
                'Empty or constant-return bytecode is a review signal, not proof of a stub.',
                'Access control, invocation opcode/static compatibility, default-method ambiguity, '
                'verification, resource availability and dynamic/reflection targets are not checked.',
                'No host JDK classes are consulted; modern dynamic-linking features are not verified.'
            ],
            summary=dict(game_classes=len(self.game), scanned_classes=len(self.scanned),
                missing_classes=len(self.missing), unresolved_methods=len(unresolved_methods),
                unresolved_fields=len(unresolved_fields), native_methods=len(native),
                abstract_methods=len(abstract), trivial_methods=len(trivial),
                parse_errors=len(self.parse_errors)),
            classes=[dict(name=name, source=definition.source,
                origin='game' if definition is self.game.get(name) else 'bundled',
                superclass=definition.superclass, interfaces=definition.interfaces,
                access_flags=definition.access, major_version=definition.major,
                declared_methods=len(definition.methods), declared_fields=len(definition.fields))
                for name, definition in sorted(self.scanned.items())],
            missing_classes=[dict(name=name, referenced_by=sorted(owners))
                             for name, owners in sorted(self.missing.items())],
            unresolved_methods=unresolved(unresolved_methods),
            unresolved_fields=unresolved(unresolved_fields),
            resolved_methods=resolved, native_methods=native, abstract_methods=abstract,
            trivial_methods=trivial, parse_errors=self.parse_errors,
            warnings=sorted(set(self.warnings)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('jar', type=Path)
    parser.add_argument('--json', type=Path, metavar='OUTPUT', help='write the full JSON report')
    parser.add_argument('--classes', type=Path, default=ROOT / 'romfs/classes',
                        help='bundled class directory (default: project romfs/classes)')
    args = parser.parse_args()
    try:
        if not args.classes.is_dir():
            raise OSError('bundled class directory does not exist: ' + str(args.classes))
        report = Auditor(args.jar, args.classes).audit()
        if args.json:
            args.json.parent.mkdir(parents=True, exist_ok=True)
            args.json.write_text(json.dumps(report, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')
    except (OSError, zipfile.BadZipFile, RuntimeError) as error:
        print('Audit error: ' + str(error), file=sys.stderr)
        return 2
    summary = report['summary']
    print('ABI ' + ('COMPLETE' if report['abi_complete'] else 'INCOMPLETE') + ': ' + str(args.jar))
    print('Classes: {game_classes} game, {scanned_classes} scanned; missing: {missing_classes} '
          'classes, {unresolved_methods} methods, {unresolved_fields} fields'.format(**summary))
    print('Behavior UNVERIFIED: {native_methods} native, {abstract_methods} abstract, '
          '{trivial_methods} empty/constant-return method declarations'.format(**summary))
    for item in report['missing_classes']:
        print('MISSING CLASS ' + item['name'] + ' <- ' + ', '.join(item['referenced_by']))
    for kind in ('methods', 'fields'):
        for item in report['unresolved_' + kind]:
            print('UNRESOLVED ' + kind[:-1].upper() + ' ' + item['owner'] + '.'
                  + item['name'] + item['descriptor'] + ' <- ' + ', '.join(item['referenced_by']))
    for item in report['parse_errors']:
        print('PARSE ERROR ' + item['source'] + ': ' + item['error'])
    for warning in report['warnings']:
        print('WARNING ' + warning)
    if args.json:
        print('JSON: ' + str(args.json))
    return 0 if report['abi_complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
