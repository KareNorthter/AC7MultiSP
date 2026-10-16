"""旧三 SP 安装入口；仅转发当前安装器，不再导入历史脚本。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from install import main
if __name__ == '__main__':
    main()
