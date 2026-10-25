#!/usr/bin/env python3
"""Generate an isolated synthetic 5000-game menu fixture and production stat helper."""
from pathlib import Path
import argparse,json,zipfile
p=argparse.ArgumentParser();p.add_argument('fixture_directory',type=Path);a=p.parse_args()
source=Path(__file__).resolve().parents[1]
text=(source/'src/platform/switch_host.inc').read_text()
helpers=text[text.index('static std::string instant_state_path('):text.index('#include "../instant_state.inc"')]
(source/'build').mkdir(exist_ok=True)
(source/'build/menu_stat.inc').write_text(helpers)
root=a.fixture_directory/'sdmc:/switch/j2me';(root/'games').mkdir(parents=True,exist_ok=False);(root/'state').mkdir()
for i in range(5000):
    with zipfile.ZipFile(root/'games'/f'g{i:05}.jar','w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('META-INF/MANIFEST.MF','MIDlet-Name: 合成游戏\r\n')
# 0.2.107: games/ is the only scanned folder. g00000 carries the Chinese name; a JAR left in the
# root must not be listed.
with zipfile.ZipFile(root/'games'/'g00000.jar','w',compression=zipfile.ZIP_STORED) as z:z.writestr('META-INF/MANIFEST.MF','MIDlet-Name: 根目录中文名\r\n')
with zipfile.ZipFile(root/'root-only.jar','w',compression=zipfile.ZIP_STORED) as z:z.writestr('META-INF/MANIFEST.MF','MIDlet-Name: 不应出现\r\n')
print(json.dumps({'synthetic_jars':5001,'unique_games':5000,'root_decoy':1,'commercial_jars':False}))
