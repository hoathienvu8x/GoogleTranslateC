CC = gcc
LDFLAGS = -I. -ldl -lpthread -lcurl -lsqlite3
ifeq ($(build),release)
	CFLAGS = -O3
	LDFLAGS += -DNDEBUG=1
else
	CFLAGS = -Og -g
endif
CFLAGS += -std=c11 -Wall -Wextra -Werror -pedantic
RM = rm -rf

OBJECTS = cgtranslate.o sha1.o
OBJECTS := $(addprefix objects/,$(OBJECTS))
EXECUTABLE = translate

all: objects $(EXECUTABLE)

objects:
	@echo "Create 'objects' folder ..."
	@mkdir -p objects

$(EXECUTABLE): objects/main.o $(OBJECTS)
ifeq ($(build),release)
	@echo "Build release '$@' executable ..."
else
	@echo "Build '$@' executable ..."
endif
	@$(CC) objects/main.o $(OBJECTS) -o $@ $(LDFLAGS)
	@$(RM) objects/main.o

objects/%.o: %.c
	@echo "Build '$@' object ..."
	@$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@echo "Cleanup ..."
	@$(RM) $(OBJECTS) $(EXECUTABLE)
