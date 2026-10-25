import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from check_switch_env import inspect_toolchain, LIBRARIES, ABI


class EnvironmentChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.a64 = self.root / 'custom-a64'
        self.env = patch.dict(os.environ, {'DEVKITA64': str(self.a64)})
        self.env.start(); self.addCleanup(self.env.stop)
        paths = ['tools/bin/build_romfs', 'tools/bin/elf2nro', 'tools/bin/nacptool',
                 'libnx/include/switch.h', 'libnx/switch.specs', 'libnx/switch.ld',
                 'libnx/lib/libnx.a', 'libnx/include/deko3d.h', 'libnx/lib/libdeko3d.a']
        paths += ['portlibs/switch/lib/lib' + name + '.a' for name in LIBRARIES]
        paths += ['custom-a64/bin/aarch64-none-elf-gcc', 'custom-a64/bin/aarch64-none-elf-g++']
        for name in paths:
            p = self.root / name;p.parent.mkdir(parents=True, exist_ok=True);p.write_bytes(b'test')
        self.headers = {}
        for name, version in ABI.items():
            path = self.root / ('portlibs/switch/include/lib' + name) / ('version.h' if name == 'avutil' else 'version_major.h')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#define LIB' + name.upper() + '_VERSION_MAJOR ' + str(version) + '\n')
            self.headers[name] = path
        self.output = patch('check_switch_env.subprocess.check_output', side_effect=lambda cmd, **kwargs: 'aarch64-none-elf\n' if cmd[1] == '-dumpmachine' else 'test-compiler\n')
        self.output.start();self.addCleanup(self.output.stop)

    def test_custom_compiler_location(self):
        result = inspect_toolchain(self.root)
        self.assertTrue(result['passed'])
        self.assertEqual(result['devkita64'], str(self.a64))
        self.assertEqual(result['ffmpeg_header_abi'], ABI)

    def test_missing_library(self):
        (self.root / 'portlibs/switch/lib/libavcodec.a').unlink()
        with self.assertRaisesRegex(ValueError, 'Missing Switch dependencies'):
            inspect_toolchain(self.root)

    def test_incompatible_abi(self):
        self.headers['avcodec'].write_text('#define LIBAVCODEC_VERSION_MAJOR 62\n')
        with self.assertRaisesRegex(ValueError, 'ABI mismatch'):
            inspect_toolchain(self.root)

    def test_missing_header(self):
        self.headers['avutil'].unlink()
        with self.assertRaisesRegex(ValueError, 'Missing FFmpeg header'):
            inspect_toolchain(self.root)

    def test_cpu_only_does_not_drop_audio(self):
        (self.root / 'libnx/lib/libdeko3d.a').unlink()
        self.assertTrue(inspect_toolchain(self.root, gpu=False)['passed'])
        (self.root / 'portlibs/switch/lib/libavformat.a').unlink()
        with self.assertRaises(ValueError):
            inspect_toolchain(self.root, gpu=False)


if __name__ == '__main__':
    unittest.main()
