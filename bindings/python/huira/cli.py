import os
import sys
import subprocess

def main():
    binary = os.path.join(os.path.dirname(__file__), "huira_bin")
    sys.exit(subprocess.call([binary] + sys.argv[1:]))
