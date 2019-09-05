PROGRAM := iotsigma_gateway
OBJDIR	:= obj
SRCEXTS := .c
CFLAGS += -g 
CFLAGS += -DDEBUG 
CFLAGS += -errchk=longptr64 
CFLAGS += -Wno-int-to-pointer-cast 
CFLAGS += -Wno-pointer-to-int-cast
RM = rm -rf

SOURCES = $(shell find ./ -maxdepth 1 -name "*$(SRCEXTS)")
OBJS = $(foreach x,$(SRCEXTS), $(patsubst ./%$(x),$(OBJDIR)/%.o,$(filter %$(x),$(SOURCES))))
OBJDIRS	= $(sort $(dir $(OBJS)))
DEPS = $(patsubst %.o,%.d,$(OBJS))

.PHONY : all clean

all : $(PROGRAM)

include $(DEPS)

$(OBJDIR)/%.d : %$(SRCEXTS)
	mkdir -p $(OBJDIR)
	$(CC) -o $@  $(CFLAGS) -MM -MD -MT '$(OBJDIR)/$(patsubst %.c,%.o,$<)' $<
$(OBJDIR)/main.o : main.c
	$(CC) -o $@ -c $(CFLAGS) $<
$(OBJDIR)/%.o : %$(SRCEXTS) %.h
	$(CC) -o $@ -c $(CFLAGS) $<
$(PROGRAM) : $(OBJS)
	$(CC) -o $(PROGRAM) $(OBJS) $(LDFLAGS)

clean :
	$(RM) $(OBJDIR)/* $(PROGRAM)
