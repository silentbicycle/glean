# Installation base path.
PREFIX= /usr/local/

# If the OS is not recognized, you may need to uncomment one of these.
#COPTS += -DHAS_GETLINE
#COPTS += -DHAS_FGETLN

# Where check in installed; only used for tests.
CHECK_INC_PATH= ${PREFIX}include/
CHECK_LIB_PATH= ${PREFIX}lib/

#########################################################
# Probably will not need to change anything after here. #
#########################################################

# Eliminate asserts (for performance).
# DEBUG += -DNDEBUG

# Add profiling annotations.
# PROFILE += -pg

# Add debugging symbols.
DEBUG += -g

COPTS+= -O3 ${DEBUG}
