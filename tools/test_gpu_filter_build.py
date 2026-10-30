#!/usr/bin/env python3
"""Build entry-point regression, without compiling the VM or touching APIs."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch
import build

def main():
    assert getattr(build,'NACP_VERSION','')=='0.2.114-k5-gpf5','NACP needs an identifiable short GPU build version'
    assert len(build.NACP_VERSION.encode('utf-8'))<=15,'NACP display version must fit without truncation'
    with tempfile.TemporaryDirectory(prefix='frozen-java-') as temporary:
        root=Path(temporary);(root/'assets-reuse.json').write_text('{"files":[]}')
        classes=root/'romfs/classes';classes.mkdir(parents=True)
        sentinel=classes/'frozen.class';sentinel.write_bytes(b'original-class-bytes')
        def forbidden(*args):raise AssertionError('frozen build invoked Java or examples')
        for target in ('desktop','switch'):
            argv=['build.py','--frozen-java']+(['--switch'] if target=='switch' else [])
            with contextlib.ExitStack() as stack:
                stack.enter_context(patch.object(sys,'argv',argv))
                stack.enter_context(patch.object(build,'ROOT',root))
                stack.enter_context(patch.object(build,'BUILD',root/'build'))
                for name in ('java_compiler','rebuild_java','rebuild_examples'):
                    stack.enter_context(patch.object(build,name,forbidden))
                stack.enter_context(patch.object(build,'switch_tools',return_value=(root,{})))
                stack.enter_context(patch('check_switch_env.inspect_toolchain',return_value={'passed':True}))
                stack.enter_context(patch.object(build,'executable',side_effect=str))
                invoked=stack.enter_context(patch.object(build,target))
                assert build.main()==0
                invoked.assert_called_once()
                assert sentinel.read_bytes()==b'original-class-bytes'
        # Both actual modules must be staged; no example games enter ROMFS.
        for directory in ('font','skins','ui','shaders'):(root/'romfs'/directory).mkdir()
        for stage in ('vert','frag'):(root/'romfs/shaders'/('filter.'+stage+'.dksh')).write_bytes(b'DKSH-fixture')
        with patch.object(build,'ROOT',root):
            staged=build.stage_switch_romfs(root/'release')
            assert (staged/'classes/frozen.class').read_bytes()==b'original-class-bytes'
            assert sorted(x.name for x in (staged/'shaders').iterdir())==['filter.frag.dksh','filter.vert.dksh']
            assert not (staged/'jar').exists()
    print('PASS frozen desktop/Switch entry points skip all Java work and preserve classes; production ROMFS stages both offline shaders without JARs')
if __name__=='__main__':main()
