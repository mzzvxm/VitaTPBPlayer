# --- Título e ID do Projeto ---
PROJECT_TITLE := VitaTPBPlayer
PROJECT_TITLEID := VTPB00001
PROJECT := stremio_vita

# --- Ferramentas de Compilação ---
CC := arm-vita-eabi-gcc
CXX := arm-vita-eabi-g++
STRIP := arm-vita-eabi-strip

# --- Função para encontrar arquivos recursivamente ---
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# --- Obter Flags e Bibliotecas do VitaSDK (CORRIGIDO) ---
# Tenta usar pkg-config. Se falhar, usa o $VITASDK como fallback.
VITASDK_CFLAGS := $(shell pkg-config --cflags vitasdk 2>/dev/null)
VITASDK_LIBS := $(shell pkg-config --libs vitasdk 2>/dev/null)

ifeq ($(VITASDK_CFLAGS),)
    $(info ### pkg-config not found or vitasdk not configured. Falling back to $VITASDK. ###)
    VITASDK_CFLAGS := -I$(VITASDK)/arm-vita-eabi/include
endif
ifeq ($(VITASDK_LIBS),)
    VITASDK_LIBS := -L$(VITASDK)/arm-vita-eabi/lib
endif

# --- Flags de Compilação ---
CFLAGS += $(VITASDK_CFLAGS) -Wl,-q -Isrc -D__VITA__
CXXFLAGS += $(VITASDK_CFLAGS) -Wl,-q -std=c++11 -Isrc -D__VITA__

# --- Arquivos Fonte e Objetos ---
SRC_C := $(call rwildcard, src/, *.c)
SRC_CPP := $(call rwildcard, src/, *.cpp)
# Usa 'uniq' para evitar o aviso de 'target given more than once' (CORRIGIDO)
OBJ_DIRS := $(sort $(addprefix out/, $(dir $(SRC_C:src/%.c=%.o)) $(dir $(SRC_CPP:src/%.cpp=%.o))))
OBJS := $(addprefix out/, $(SRC_C:src/%.c=%.o) $(SRC_CPP:src/%.cpp=%.o))

# --- Bibliotecas para Linkagem (CORRIGIDO) ---
# Adicionada a biblioteca -lSceCtrl_stub para as funções de controle
LIBS = -lSceDisplay_stub -lSceSysmem_stub -lSceAppMgr_stub -lSceLibKernel_stub \
       -lSceCtrl_stub \
       -lcurl -lssl -lcrypto -lz -lm \
       $(VITASDK_LIBS)

# --- Regras de Compilação ---
all: package

package: $(PROJECT).vpk

$(PROJECT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		$@

eboot.bin: $(PROJECT).velf
	vita-make-fself $< $@

param.sfo:
	vita-mksfoex -s TITLE_ID=$(PROJECT_TITLEID) "$(PROJECT_TITLE)" $@

$(PROJECT).velf: $(PROJECT).elf
	$(STRIP) -g $<
	vita-elf-create $< $@

$(PROJECT).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(OBJ_DIRS):
	mkdir -p $@

out/%.o : src/%.c | $(OBJ_DIRS)
	$(CC) -c $(CFLAGS) -o $@ $<

out/%.o : src/%.cpp | $(OBJ_DIRS)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(PROJECT).velf $(PROJECT).elf $(PROJECT).vpk param.sfo eboot.bin out

.PHONY: all package clean