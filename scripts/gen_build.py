###############################################################################
#                               imports                                       #
###############################################################################

import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../pl_build")

import pl_build as pl

# this will run all the builds
import gen_core
import gen_examples
import gen_tests
