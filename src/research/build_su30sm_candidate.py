"""旧入口兼容转发；所有构建规则集中在 src/build.py。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build import main
if __name__ == '__main__':
    main()
