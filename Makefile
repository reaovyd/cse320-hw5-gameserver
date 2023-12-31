CC := gcc
SRCD := src
TSTD := tests
BLDD := build
BIND := bin
INCD := include
LIBD := lib
UTILD := util

MAIN  := $(BLDD)/main.o
LIB := $(LIBD)/jeux.a
LIB_DB := $(LIBD)/jeux_debug.a

ALL_SRCF := $(shell find $(SRCD) -type f -name *.c)
ALL_OBJF := $(patsubst $(SRCD)/%,$(BLDD)/%,$(ALL_SRCF:.c=.o))
ALL_FUNCF := $(filter-out $(MAIN), $(ALL_OBJF))

TEST_SRC := $(shell find $(TSTD) -type f -name *.c)

INC := -I $(INCD)

CFLAGS := -Wall -Werror -Wno-unused-function -MMD -fcommon
DFLAGS := -g -DDEBUG -DCOLOR
PRINT_STAMENTS := -DERROR -DSUCCESS -DWARN -DINFO

STD := -std=gnu11
TEST_LIB := -lcriterion
LIBS := $(LIB) -lpthread -lm
LIBS_DB := $(LIB_DB) -lpthread -lm
EXCLUDES := excludes.h

CFLAGS += $(STD)

EXEC := jeux
TEST_EXEC := $(EXEC)_tests
CLIENT_EXEC := jclient

.PHONY: clean all setup debug

all: setup $(BIND)/$(EXEC) $(INCD)/$(EXCLUDES) $(BIND)/$(TEST_EXEC)

debug: CFLAGS += $(DFLAGS) $(PRINT_STAMENTS)
debug: LIBS := $(LIBS_DB)
debug: all

setup: $(BIND) $(BLDD)
$(BIND):
	mkdir -p $(BIND)
$(BLDD):
	mkdir -p $(BLDD)

$(BIND)/$(EXEC): $(MAIN) $(ALL_FUNCF)
	$(CC) $^ -o $@ $(LIBS)

$(BIND)/$(TEST_EXEC): $(ALL_FUNCF) $(TEST_SRC)
	$(CC) $(CFLAGS) $(INC) $(ALL_FUNCF) $(TEST_SRC) $(TEST_LIB) $(LIBS) -o $@

$(BLDD)/%.o: $(SRCD)/%.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -rf $(BLDD) $(BIND)

$(INCD)/$(EXCLUDES): $(BIND)/$(EXEC)
	rm -f $@
	touch $@
	if nm $(BIND)/$(EXEC) | grep 'INSTRUCTOR_CLIENT$$' > /dev/null; then \
           echo "#define NO_CLIENT" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_CLIENT_REGISTRY > /dev/null; then \
           echo "#define NO_CLIENT_REGISTRY" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_GAME > /dev/null; then \
           echo "#define NO_GAME" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_INVITATION > /dev/null; then \
           echo "#define NO_INVITATION" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_MAIN > /dev/null; then \
           echo "#define NO_MAIN" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep 'INSTRUCTOR_PLAYER$$' > /dev/null; then \
           echo "#define NO_PLAYER" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_PLAYER_REGISTRY > /dev/null; then \
           echo "#define NO_PLAYER_REGISTRY" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_PROTOCOL > /dev/null; then \
           echo "#define NO_PROTOCOL" >> $@; \
        fi
	if nm $(BIND)/$(EXEC) | grep INSTRUCTOR_SERVER > /dev/null; then \
           echo "#define NO_SERVER" >> $@; \
        fi

.PRECIOUS: $(BLDD)/*.d
-include $(BLDD)/*.d
