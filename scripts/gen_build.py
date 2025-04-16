#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + "/..")
import build.core as pl

# this will run all the builds
import gen_dev
import gen_examples
import gen_tests
