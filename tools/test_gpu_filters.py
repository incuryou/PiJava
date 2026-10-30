#!/usr/bin/env python3
"""Host contracts for GPU presentation. This does not execute a GPU shader."""
import argparse
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    assert (ROOT/'src/presentation/gpu_filter.h').is_file(), 'GPU filter raw-input/cache contract not implemented'
    out=ROOT/'build/gpu-filter-evidence';out.mkdir(parents=True,exist_ok=True)
    flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    for name in ('gpu_filter_checks','gpu_adaptive_checks'):
        exe=out/(name+('-san' if args.sanitize else ''))
        subprocess.run(['g++','-std=c++17']+flags+[str(ROOT/'tests'/(name+'.cpp')),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()
