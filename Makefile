TITLE_ID = VTPB00001
TARGET   = VitaTPBPlayer
OBJS     = main.o ui.o osk.o player.o realdebrid.o tpb_scraper.o token_server.o stream_manager.o cJSON.o

# MODIFICADO: Adicionada a biblioteca -lSceTouch_stub
LIBS = -lvita2d -lSceAppUtil_stub -lSceCommonDialog_stub -lSceGxm_stub -lSceDisplay_stub \
       -lSceSysmodule_stub -lSceCtrl_stub -lSceIme_stub -lSceTouch_stub -lScePgf_stub -lScePvf_stub \
       -lfreetype -lpng -ljpeg -lz -lbz2 -lm -lc \
       -lcurl -lssl -lcrypto -lSceAppMgr_stub \
       -lSceNet_stub -lSceNetCtl_stub -lpthread

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3

CFLAGS += -I$(VITASDK)/arm-vita-eabi/include -Isrc -D__VITA__
LIBS   += -L$(VITASDK)/arm-vita-eabi/lib

all: package

package: $(TARGET).vpk

$(TARGET).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add resources/font.ttf=font.ttf \
		--add resources/cacert.pem=cacert.pem \
		$@

eboot.bin: $(TARGET).velf
	vita-make-fself -s $< $@

param.sfo:
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "$(TARGET)" $@

$(TARGET).velf: $(TARGET).elf
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) \
		eboot.bin param.sfo out

.PHONY: all package clean