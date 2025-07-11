

all: scratchbird

.DEFAULT:
	$(MAKE) -C gen $@
