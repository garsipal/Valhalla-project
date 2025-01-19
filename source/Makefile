CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math
override CXXFLAGS+= -Wall -Wimplicit-fallthrough -fsigned-char -fno-exceptions -fno-rtti

PLATFORM= $(shell uname -s | tr '[:lower:]' '[:upper:]')
PLATFORM_ARCH= $(shell uname -m)
PLATFORM_PREFIX= native

INCLUDES= -Ishared -Iengine -Igame -Ienet/include

STRIP=
ifeq (,$(findstring -g,$(CXXFLAGS)))
ifeq (,$(findstring -pg,$(CXXFLAGS)))
  STRIP=strip
endif
endif

MV=mv

ifneq (,$(findstring MINGW,$(PLATFORM)))
WINDRES= windres
ifneq (,$(findstring 64,$(PLATFORM)))
ifneq (,$(findstring CROSS,$(PLATFORM)))
  CXX=x86_64-w64-mingw32-g++
  WINDRES=x86_64-w64-mingw32-windres
ifneq (,$(STRIP))
  STRIP=x86_64-w64-mingw32-strip
endif
endif
WINLIB=lib64
WINBIN=../bin/bin64
override CXX+= -m64
override WINDRES+= -F pe-x86-64
else
ifneq (,$(findstring CROSS,$(PLATFORM)))
  CXX=i686-w64-mingw32-g++
  WINDRES=i686-w64-mingw32-windres
ifneq (,$(STRIP))
  STRIP=i686-w64-mingw32-strip
endif
endif
WINLIB=lib
WINBIN=../bin/bin32
override CXX+= -m32
override WINDRES+= -F pe-i386
endif
CLIENT_INCLUDES= $(INCLUDES) -Iinclude
STD_LIBS= -static-libgcc -static-libstdc++
CLIENT_LIBS= -mwindows $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -lSDL2 -lSDL2_image -lSDL2_mixer -lzlib1 -lopengl32 -lenet -lws2_32 -lwinmm
else
ifneq (,$(findstring DARWIN,$(PLATFORM)))
OSXMIN= 10.9
override CC+=-arch x86_64 -mmacosx-version-min=$(OSXMIN)
override CXX+=-arch x86_64 -mmacosx-version-min=$(OSXMIN)
CLIENT_INCLUDES= $(INCLUDES) -Iinclude
CLIENT_LIBS= -F../bin/valhalla.app/Contents/Frameworks/ -framework SDL2 -framework SDL2_image
CLIENT_LIBS+= -framework SDL2_mixer -framework CoreAudio -framework AudioToolbox
CLIENT_LIBS+= -framework AudioUnit -framework OpenGL -framework Cocoa -lz -Lenet -lenet
else
CLIENT_INCLUDES= $(INCLUDES) -I/usr/X11R6/include `sdl2-config --cflags` `pkg-config --cflags pangocairo` `pkg-config --cflags fontconfig`
CLIENT_LIBS= -Lenet -lenet -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer `pkg-config --libs pangocairo` `pkg-config --libs fontconfig` -lz -lGL
endif
endif
ifeq ($(PLATFORM),LINUX)
CLIENT_LIBS+= -lrt
else
ifneq (,$(findstring GNU,$(PLATFORM))) 
CLIENT_LIBS+= -lrt 
endif 
endif

CLIENT_OBJS= \
	shared/crypto.o \
	shared/geom.o \
	shared/glemu.o \
	shared/stream.o \
	shared/tools.o \
	shared/zip.o \
	engine/aa.o \
	engine/bih.o \
	engine/blend.o \
	engine/client.o	\
	engine/command.o \
	engine/console.o \
	engine/dynlight.o \
	engine/grass.o \
	engine/light.o \
	engine/main.o \
	engine/material.o \
	engine/menus.o \
	engine/movie.o \
	engine/normal.o	\
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/physics.o \
	engine/pvs.o \
	engine/rendergl.o \
	engine/renderlights.o \
	engine/rendermodel.o \
	engine/renderparticles.o \
	engine/rendersky.o \
	engine/rendertext.o \
	engine/renderva.o \
	engine/server.o	\
	engine/serverbrowser.o \
	engine/shader.o \
	engine/sound.o \
	engine/stain.o \
	engine/texture.o \
	engine/ui.o \
	engine/liquid.o \
	engine/world.o \
	engine/worldio.o \
	game/ai.o \
	game/gameclient.o \
	game/entity.o \
	game/game.o \
	game/render.o \
	game/scoreboard.o \
	game/gameserver.o \
	game/waypoint.o \
	game/monster.o \
	game/weapon.o \
	game/gamephysics.o \
	game/hud.o \
	game/projectile.o \
	game/announcer.o \
	game/camera.o

CLIENT_PCH= shared/cube.h.gch engine/engine.h.gch game/game.h.gch

SERVER_INCLUDES= -DSTANDALONE -Istandalone/shared -Istandalone/engine -Istandalone/game $(INCLUDES)
ifneq (,$(findstring MINGW,$(PLATFORM)))
SERVER_INCLUDES+= -Iinclude
SERVER_LIBS= -mwindows $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -lzlib1 -lenet -lws2_32 -lwinmm
MASTER_LIBS= $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -lzlib1 -lenet -lws2_32 -lwinmm
else
SERVER_LIBS= -Lenet -lenet -lz
MASTER_LIBS= $(SERVER_LIBS)
MAXMINDDB_SUPPORT:= $(shell ./geoip/geoip_check.sh $(CXX) $(CXXFLAGS) $(SERVER_INCLUDES))
SERVER_INCLUDES+= $(MAXMINDDB_SUPPORT)
ifneq (,$(findstring HAVE_MAXMINDDB,$(MAXMINDDB_SUPPORT)))
SERVER_LIBS+= -lmaxminddb
endif
endif

SERVER_OBJS= \
	standalone/shared/crypto.o \
	standalone/shared/stream.o \
	standalone/shared/tools.o \
	standalone/engine/command.o \
	standalone/engine/server.o \
	standalone/engine/worldio.o \
	standalone/game/entity.o \
	standalone/game/gameserver.o

SERVER_PCH= standalone/shared/cube.h.gch standalone/engine/engine.h.gch standalone/game/game.h.gch

MASTER_OBJS= \
	standalone/shared/crypto.o \
	standalone/shared/stream.o \
	standalone/shared/tools.o \
	standalone/engine/command.o \
	standalone/engine/master.o

SERVER_MASTER_OBJS= $(SERVER_OBJS) $(filter-out $(SERVER_OBJS),$(MASTER_OBJS))

default: all

all: client server

clean:
	-$(RM) $(CLIENT_PCH) $(CLIENT_OBJS) $(SERVER_PCH) $(SERVER_MASTER_OBJS) tess_client tess_server tess_master

fixspace:
	sed -i 's/[ \t]*$$//; :rep; s/^\([ ]*\)\t/\1    /g; trep' shared/*.c shared/*.cpp shared/*.h engine/*.cpp engine/*.h game/*.cpp game/*.h

$(filter-out shared/%,$(CLIENT_PCH)): $(filter shared/%,$(CLIENT_PCH))
$(CLIENT_PCH): %.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $@.tmp $<
	$(MV) $@.tmp $@

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(filter shared/%,$(CLIENT_OBJS)): $(filter shared/%,$(CLIENT_PCH))
$(filter engine/%,$(CLIENT_OBJS)): $(filter engine/%,$(CLIENT_PCH))
$(filter game/%,$(CLIENT_OBJS)): $(filter game/%,$(CLIENT_PCH))

$(filter-out standalone/shared/%,$(SERVER_PCH)): $(filter standalone/shared/%,$(SERVER_PCH))
$(SERVER_PCH): standalone/%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $@.tmp $<
	$(MV) $@.tmp $@

$(SERVER_MASTER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)
$(filter standalone/shared/%,$(SERVER_MASTER_OBJS)): $(filter standalone/shared/%,$(SERVER_PCH))
$(filter standalone/engine/%,$(SERVER_MASTER_OBJS)): $(filter standalone/engine/%,$(SERVER_PCH))
$(filter standalone/game/%,$(SERVER_MASTER_OBJS)): $(filter standalone/game/%,$(SERVER_PCH))

$(SERVER_MASTER_OBJS): standalone/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

ifneq (,$(findstring MINGW,$(PLATFORM)))
client: $(CLIENT_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff 
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tesseract.exe vcpp/mingw.res $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tess_server.exe vcpp/mingw.res $(SERVER_OBJS) $(SERVER_LIBS)

master: $(MASTER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/tess_master.exe $(MASTER_OBJS) $(MASTER_LIBS)

install: all
else
client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_client $(CLIENT_OBJS) $(CLIENT_LIBS)
ifneq (,$(findstring DARWIN,$(PLATFORM)))
	install_name_tool -add_rpath @executable_path/../Frameworks tess_client
endif

server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
master: libenet $(MASTER_OBJS)
	$(CXX) $(CXXFLAGS) -o tess_master $(MASTER_OBJS) $(MASTER_LIBS)  

ifneq (,$(findstring DARWIN,$(PLATFORM)))
install: client
	cp tess_client ../bin/valhalla.app/Contents/MacOS/valhalla_universal
else
install: all
	cp -f tess_client	../bin/bin_unix/$(PLATFORM_PREFIX)_client
	cp -f tess_server	../bin/bin_unix/$(PLATFORM_PREFIX)_server
ifneq (,$(STRIP))
	$(STRIP) ../bin/bin_unix/$(PLATFORM_PREFIX)_client
	$(STRIP) ../bin/bin_unix/$(PLATFORM_PREFIX)_server
endif
endif
endif

enet/libenet.a:
	$(MAKE) -C enet
libenet: enet/libenet.a

depend:
	makedepend -Y -Ishared -Iengine -Igame $(CLIENT_OBJS:.o=.cpp)
	makedepend -a -o.h.gch -Y -Ishared -Iengine -Igame $(CLIENT_PCH:.h.gch=.h)
	makedepend -a -pstandalone/ -Y -DSTANDALONE -Ishared -Iengine -Igame $(SERVER_MASTER_OBJS:standalone/%.o=%.cpp)
	makedepend -a -pstandalone/ -o.h.gch -Y -DSTANDALONE -Ishared -Iengine -Igame $(SERVER_PCH:standalone/%.h.gch=%.h)

# DO NOT DELETE

shared/crypto.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/crypto.o: shared/command.h shared/glexts.h shared/glemu.h
shared/crypto.o: engine/sound.h shared/iengine.h shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/geom.o: shared/command.h shared/glexts.h shared/glemu.h engine/sound.h
shared/geom.o: shared/iengine.h shared/igame.h
shared/glemu.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/glemu.o: shared/command.h shared/glexts.h shared/glemu.h
shared/glemu.o: engine/sound.h shared/iengine.h shared/igame.h
shared/stream.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/stream.o: shared/command.h shared/glexts.h shared/glemu.h
shared/stream.o: engine/sound.h shared/iengine.h shared/igame.h
shared/tools.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/tools.o: shared/command.h shared/glexts.h shared/glemu.h
shared/tools.o: engine/sound.h shared/iengine.h shared/igame.h
shared/tools.o: shared/unicode.h
shared/zip.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/zip.o: shared/command.h shared/glexts.h shared/glemu.h engine/sound.h
shared/zip.o: shared/iengine.h shared/igame.h
engine/aa.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/aa.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/aa.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/aa.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/aa.o: engine/model.h
engine/bih.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/bih.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/bih.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/bih.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/bih.o: engine/model.h
engine/blend.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/blend.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/blend.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/blend.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/blend.o: engine/model.h
engine/client.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/client.o: shared/ents.h shared/command.h shared/glexts.h
engine/client.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/client.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/client.o: engine/texture.h engine/bih.h engine/model.h
engine/command.o: shared/unicode.h shared/cube.h shared/tools.h shared/geom.h
engine/command.o: shared/ents.h shared/command.h shared/glexts.h
engine/command.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/command.o: shared/igame.h engine/engine.h engine/world.h engine/octa.h
engine/command.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/console.o: shared/unicode.h shared/cube.h shared/tools.h shared/geom.h
engine/console.o: shared/ents.h shared/command.h shared/glexts.h
engine/console.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/console.o: shared/igame.h engine/engine.h engine/world.h engine/octa.h
engine/console.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/dynlight.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/dynlight.o: shared/ents.h shared/command.h shared/glexts.h
engine/dynlight.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/dynlight.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/dynlight.o: engine/texture.h engine/bih.h engine/model.h
engine/grass.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/grass.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/grass.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/grass.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/grass.o: engine/model.h
engine/light.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/light.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/light.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/light.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/light.o: engine/model.h
engine/main.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/main.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/main.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/main.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/main.o: engine/model.h engine/version.h
engine/material.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/material.o: shared/ents.h shared/command.h shared/glexts.h
engine/material.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/material.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/material.o: engine/texture.h engine/bih.h engine/model.h
engine/menus.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/menus.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/menus.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/menus.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/menus.o: engine/model.h
engine/movie.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/movie.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/movie.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/movie.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/movie.o: engine/model.h
engine/normal.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/normal.o: shared/ents.h shared/command.h shared/glexts.h
engine/normal.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/normal.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/normal.o: engine/texture.h engine/bih.h engine/model.h
engine/octa.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/octa.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/octa.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/octa.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/octa.o: engine/model.h
engine/octaedit.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/octaedit.o: shared/ents.h shared/command.h shared/glexts.h
engine/octaedit.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/octaedit.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/octaedit.o: engine/texture.h engine/bih.h engine/model.h
engine/octarender.o: engine/engine.h shared/cube.h shared/tools.h
engine/octarender.o: shared/geom.h shared/ents.h shared/command.h
engine/octarender.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/octarender.o: shared/iengine.h shared/igame.h engine/world.h
engine/octarender.o: engine/octa.h engine/light.h engine/texture.h
engine/octarender.o: engine/bih.h engine/model.h
engine/physics.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/physics.o: shared/ents.h shared/command.h shared/glexts.h
engine/physics.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/physics.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/physics.o: engine/texture.h engine/bih.h engine/model.h engine/mpr.h
engine/pvs.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/pvs.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/pvs.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/pvs.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/pvs.o: engine/model.h
engine/rendergl.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/rendergl.o: shared/ents.h shared/command.h shared/glexts.h
engine/rendergl.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/rendergl.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/rendergl.o: engine/texture.h engine/bih.h engine/model.h
engine/renderlights.o: engine/engine.h shared/cube.h shared/tools.h
engine/renderlights.o: shared/geom.h shared/ents.h shared/command.h
engine/renderlights.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/renderlights.o: shared/iengine.h shared/igame.h engine/world.h
engine/renderlights.o: engine/octa.h engine/light.h engine/texture.h
engine/renderlights.o: engine/bih.h engine/model.h
engine/rendermodel.o: engine/engine.h shared/cube.h shared/tools.h
engine/rendermodel.o: shared/geom.h shared/ents.h shared/command.h
engine/rendermodel.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/rendermodel.o: shared/iengine.h shared/igame.h engine/world.h
engine/rendermodel.o: engine/octa.h engine/light.h engine/texture.h
engine/rendermodel.o: engine/bih.h engine/model.h engine/ragdoll.h
engine/rendermodel.o: engine/animmodel.h engine/vertmodel.h
engine/rendermodel.o: engine/skelmodel.h engine/hitzone.h engine/md2.h
engine/rendermodel.o: engine/md3.h engine/md5.h engine/obj.h engine/smd.h
engine/rendermodel.o: engine/iqm.h
engine/renderparticles.o: engine/engine.h shared/cube.h shared/tools.h
engine/renderparticles.o: shared/geom.h shared/ents.h shared/command.h
engine/renderparticles.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/renderparticles.o: shared/iengine.h shared/igame.h engine/world.h
engine/renderparticles.o: engine/octa.h engine/light.h engine/texture.h
engine/renderparticles.o: engine/bih.h engine/model.h engine/explosion.h
engine/renderparticles.o: engine/lensflare.h engine/lightning.h
engine/rendersky.o: engine/engine.h shared/cube.h shared/tools.h
engine/rendersky.o: shared/geom.h shared/ents.h shared/command.h
engine/rendersky.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/rendersky.o: shared/iengine.h shared/igame.h engine/world.h
engine/rendersky.o: engine/octa.h engine/light.h engine/texture.h
engine/rendersky.o: engine/bih.h engine/model.h
engine/rendertext.o: engine/engine.h shared/cube.h shared/tools.h
engine/rendertext.o: shared/geom.h shared/ents.h shared/command.h
engine/rendertext.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/rendertext.o: shared/iengine.h shared/igame.h engine/world.h
engine/rendertext.o: engine/octa.h engine/light.h engine/texture.h
engine/rendertext.o: engine/bih.h engine/model.h
engine/renderva.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/renderva.o: shared/ents.h shared/command.h shared/glexts.h
engine/renderva.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/renderva.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/renderva.o: engine/texture.h engine/bih.h engine/model.h
engine/server.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/server.o: shared/ents.h shared/command.h shared/glexts.h
engine/server.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/server.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/server.o: engine/texture.h engine/bih.h engine/model.h
engine/serverbrowser.o: engine/engine.h shared/cube.h shared/tools.h
engine/serverbrowser.o: shared/geom.h shared/ents.h shared/command.h
engine/serverbrowser.o: shared/glexts.h shared/glemu.h engine/sound.h
engine/serverbrowser.o: shared/iengine.h shared/igame.h engine/world.h
engine/serverbrowser.o: engine/octa.h engine/light.h engine/texture.h
engine/serverbrowser.o: engine/bih.h engine/model.h
engine/shader.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/shader.o: shared/ents.h shared/command.h shared/glexts.h
engine/shader.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/shader.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/shader.o: engine/texture.h engine/bih.h engine/model.h
engine/sound.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/sound.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/sound.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/sound.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/sound.o: engine/model.h
engine/stain.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/stain.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/stain.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/stain.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/stain.o: engine/model.h
engine/texture.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/texture.o: shared/ents.h shared/command.h shared/glexts.h
engine/texture.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/texture.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/texture.o: engine/texture.h engine/bih.h engine/model.h
engine/ui.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/ui.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/ui.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/ui.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/ui.o: engine/model.h engine/textedit.h shared/unicode.h
engine/liquid.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/liquid.o: shared/ents.h shared/command.h shared/glexts.h
engine/liquid.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/liquid.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/liquid.o: engine/texture.h engine/bih.h engine/model.h
engine/world.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/world.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
engine/world.o: engine/sound.h shared/iengine.h shared/igame.h engine/world.h
engine/world.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/world.o: engine/model.h
engine/worldio.o: engine/engine.h shared/cube.h shared/tools.h shared/geom.h
engine/worldio.o: shared/ents.h shared/command.h shared/glexts.h
engine/worldio.o: shared/glemu.h engine/sound.h shared/iengine.h
engine/worldio.o: shared/igame.h engine/world.h engine/octa.h engine/light.h
engine/worldio.o: engine/texture.h engine/bih.h engine/model.h
game/ai.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/ai.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/ai.o: engine/sound.h shared/iengine.h shared/igame.h shared/unicode.h
game/ai.o: game/projectile.h game/weapon.h game/ai.h game/gamemode.h
game/ai.o: game/entity.h game/monster.h
game/gameclient.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/gameclient.o: shared/ents.h shared/command.h shared/glexts.h
game/gameclient.o: shared/glemu.h engine/sound.h shared/iengine.h
game/gameclient.o: shared/igame.h shared/unicode.h game/projectile.h
game/gameclient.o: game/weapon.h game/ai.h game/gamemode.h game/entity.h
game/gameclient.o: game/monster.h game/ctf.h game/elimination.h
game/entity.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/entity.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/entity.o: engine/sound.h shared/iengine.h shared/igame.h
game/entity.o: shared/unicode.h game/projectile.h game/weapon.h game/ai.h
game/entity.o: game/gamemode.h game/entity.h game/monster.h
game/game.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/game.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/game.o: engine/sound.h shared/iengine.h shared/igame.h shared/unicode.h
game/game.o: game/projectile.h game/weapon.h game/ai.h game/gamemode.h
game/game.o: game/entity.h game/monster.h
game/render.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/render.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/render.o: engine/sound.h shared/iengine.h shared/igame.h
game/render.o: shared/unicode.h game/projectile.h game/weapon.h game/ai.h
game/render.o: game/gamemode.h game/entity.h game/monster.h
game/scoreboard.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/scoreboard.o: shared/ents.h shared/command.h shared/glexts.h
game/scoreboard.o: shared/glemu.h engine/sound.h shared/iengine.h
game/scoreboard.o: shared/igame.h shared/unicode.h game/projectile.h
game/scoreboard.o: game/weapon.h game/ai.h game/gamemode.h game/entity.h
game/scoreboard.o: game/monster.h
game/gameserver.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/gameserver.o: shared/ents.h shared/command.h shared/glexts.h
game/gameserver.o: shared/glemu.h engine/sound.h shared/iengine.h
game/gameserver.o: shared/igame.h shared/unicode.h game/projectile.h
game/gameserver.o: game/weapon.h game/ai.h game/gamemode.h game/entity.h
game/gameserver.o: game/monster.h game/geoip.h game/ctf.h game/elimination.h
game/gameserver.o: game/extinfo.h game/aimanager.h
game/waypoint.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/waypoint.o: shared/ents.h shared/command.h shared/glexts.h
game/waypoint.o: shared/glemu.h engine/sound.h shared/iengine.h
game/waypoint.o: shared/igame.h shared/unicode.h game/projectile.h
game/waypoint.o: game/weapon.h game/ai.h game/gamemode.h game/entity.h
game/waypoint.o: game/monster.h
game/monster.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/monster.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/monster.o: engine/sound.h shared/iengine.h shared/igame.h
game/monster.o: shared/unicode.h game/projectile.h game/weapon.h game/ai.h
game/monster.o: game/gamemode.h game/entity.h game/monster.h
game/weapon.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/weapon.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/weapon.o: engine/sound.h shared/iengine.h shared/igame.h
game/weapon.o: shared/unicode.h game/projectile.h game/weapon.h game/ai.h
game/weapon.o: game/gamemode.h game/entity.h game/monster.h
game/gamephysics.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/gamephysics.o: shared/ents.h shared/command.h shared/glexts.h
game/gamephysics.o: shared/glemu.h engine/sound.h shared/iengine.h
game/gamephysics.o: shared/igame.h shared/unicode.h game/projectile.h
game/gamephysics.o: game/weapon.h game/ai.h game/gamemode.h game/entity.h
game/gamephysics.o: game/monster.h
game/hud.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/hud.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/hud.o: engine/sound.h shared/iengine.h shared/igame.h shared/unicode.h
game/hud.o: game/projectile.h game/weapon.h game/ai.h game/gamemode.h
game/hud.o: game/entity.h game/monster.h engine/engine.h engine/world.h
game/hud.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/hud.o: engine/model.h
game/projectile.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/projectile.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/projectile.o: engine/sound.h shared/iengine.h shared/igame.h game/weapon.h game/projectile.h
game/projectile.o: game/ai.h game/gamemode.h game/entity.h game/monster.h
game/announcer.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/announcer.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/announcer.o: engine/sound.h shared/iengine.h shared/igame.h game/weapon.h game/projectile.h
game/announcer.o: game/ai.h game/gamemode.h game/entity.h game/monster.h
game/camera.o: game/game.h shared/cube.h shared/tools.h shared/geom.h
game/camera.o: shared/ents.h shared/command.h shared/glexts.h shared/glemu.h
game/camera.o: engine/sound.h shared/iengine.h shared/igame.h game/weapon.h game/projectile.h
game/camera.o: game/ai.h game/gamemode.h game/entity.h game/monster.h
game/camera.o: engine/engine.h engine/world.h engine/octa.h engine/light.h
game/camera.o: engine/texture.h engine/bih.h engine/model.h

shared/cube.h.gch: shared/tools.h shared/geom.h shared/ents.h
shared/cube.h.gch: shared/command.h shared/glexts.h shared/glemu.h
shared/cube.h.gch: engine/sound.h shared/iengine.h shared/igame.h
engine/engine.h.gch: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/engine.h.gch: shared/command.h shared/glexts.h shared/glemu.h
engine/engine.h.gch: engine/sound.h shared/iengine.h shared/igame.h
engine/engine.h.gch: engine/world.h engine/octa.h engine/light.h
engine/engine.h.gch: engine/texture.h engine/bih.h engine/model.h
game/game.h.gch: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
game/game.h.gch: shared/command.h shared/glexts.h shared/glemu.h
game/game.h.gch: engine/sound.h shared/iengine.h shared/igame.h
game/game.h.gch: shared/unicode.h game/projectile.h game/game.h game/weapon.h
game/game.h.gch: game/ai.h game/gamemode.h game/entity.h game/monster.h

standalone/shared/crypto.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/crypto.o: shared/ents.h shared/command.h engine/sound.h
standalone/shared/crypto.o: shared/iengine.h shared/igame.h
standalone/shared/stream.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/stream.o: shared/ents.h shared/command.h engine/sound.h
standalone/shared/stream.o: shared/iengine.h shared/igame.h
standalone/shared/tools.o: shared/cube.h shared/tools.h shared/geom.h
standalone/shared/tools.o: shared/ents.h shared/command.h engine/sound.h
standalone/shared/tools.o: shared/iengine.h shared/igame.h shared/unicode.h
standalone/engine/command.o: shared/unicode.h shared/cube.h shared/tools.h
standalone/engine/command.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/command.o: engine/sound.h shared/iengine.h shared/igame.h
standalone/engine/command.o: engine/engine.h engine/world.h
standalone/engine/server.o: engine/engine.h shared/cube.h shared/tools.h
standalone/engine/server.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/server.o: engine/sound.h shared/iengine.h shared/igame.h
standalone/engine/server.o: engine/world.h engine/version.h
standalone/engine/worldio.o: engine/engine.h shared/cube.h shared/tools.h
standalone/engine/worldio.o: shared/geom.h shared/ents.h shared/command.h
standalone/engine/worldio.o: engine/sound.h shared/iengine.h shared/igame.h
standalone/engine/worldio.o: engine/world.h
standalone/game/entity.o: game/game.h shared/cube.h shared/tools.h
standalone/game/entity.o: shared/geom.h shared/ents.h shared/command.h
standalone/game/entity.o: engine/sound.h shared/iengine.h shared/igame.h
standalone/game/entity.o: shared/unicode.h game/projectile.h game/weapon.h
standalone/game/entity.o: game/ai.h game/gamemode.h game/entity.h
standalone/game/entity.o: game/monster.h
standalone/game/gameserver.o: game/game.h shared/cube.h shared/tools.h
standalone/game/gameserver.o: shared/geom.h shared/ents.h shared/command.h
standalone/game/gameserver.o: engine/sound.h shared/iengine.h shared/igame.h
standalone/game/gameserver.o: shared/unicode.h game/projectile.h
standalone/game/gameserver.o: game/weapon.h game/ai.h game/gamemode.h
standalone/game/gameserver.o: game/entity.h game/monster.h game/geoip.h
standalone/game/gameserver.o: game/ctf.h game/elimination.h game/extinfo.h
standalone/game/gameserver.o: game/aimanager.h
standalone/engine/master.o: shared/cube.h shared/tools.h shared/geom.h
standalone/engine/master.o: shared/ents.h shared/command.h engine/sound.h
standalone/engine/master.o: shared/iengine.h shared/igame.h

standalone/shared/cube.h.gch: shared/tools.h shared/geom.h shared/ents.h
standalone/shared/cube.h.gch: shared/command.h engine/sound.h
standalone/shared/cube.h.gch: shared/iengine.h shared/igame.h
standalone/engine/engine.h.gch: shared/cube.h shared/tools.h shared/geom.h
standalone/engine/engine.h.gch: shared/ents.h shared/command.h engine/sound.h
standalone/engine/engine.h.gch: shared/iengine.h shared/igame.h
standalone/engine/engine.h.gch: engine/world.h
standalone/game/game.h.gch: shared/cube.h shared/tools.h shared/geom.h
standalone/game/game.h.gch: shared/ents.h shared/command.h engine/sound.h
standalone/game/game.h.gch: shared/iengine.h shared/igame.h shared/unicode.h
standalone/game/game.h.gch: game/projectile.h game/game.h game/weapon.h
standalone/game/game.h.gch: game/ai.h game/gamemode.h game/entity.h
standalone/game/game.h.gch: game/monster.h
