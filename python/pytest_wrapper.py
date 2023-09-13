import sys
import pytest
import os

if __name__ == "__main__":
    sys.exit(pytest.main(["--junitxml", os.environ["XML_OUTPUT_FILE"]] + sys.argv[1:]))