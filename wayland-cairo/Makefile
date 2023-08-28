CC = gcc
CXX = gcc
CXXFLAGS = -std=c++11 -MMD
LDFLAGS = -lwayland-client -lwayland-egl -lEGL -lGLESv2 -lcairo -lrt
SOURCES = \
	main.c \
	shm.c \
	xdg-shell-protocol.c \
	$(NULL)

OBJECTS = $(SOURCES:.c=.o)
DEPS = $(SOURCES:.c=.d)

wayland-cairo: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CXXFLAGS)

clean:
	$(RM) $(OBJECTS) $(DEPS) *~ wayland-cairo

-include $(DEPS)
