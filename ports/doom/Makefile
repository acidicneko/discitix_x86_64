# Doom (doomgeneric), built in build/ from the src/ submodule plus the
# Discitix backend doomgeneric_discitix.c. No sound.
#
# The shareware IWAD (doom1.wad) is downloaded on first build and installed
# to /usr/share/games/doom, where the backend looks for it by default.

include ../port.mk

SRC   := src/doomgeneric
BUILD := build
BIN   := $(BUILD)/doom

WAD     := $(BUILD)/doom1.wad
# Shareware v1.9; any mirror is fine as long as the checksum matches
WAD_URL  := https://github.com/Akbar30Bill/DOOM_wads/raw/master/doom1.wad
WAD_SHA1 := 5b2e249b9c5133ec987b3ea77596381dc0d6bc1d
WAD_DIR := /usr/share/games/doom

# The engine objects from doomgeneric's own Makefile, minus its platform
# backend (doomgeneric_xlib.o)
DOOM_OBJS := dummy am_map doomdef doomstat dstrings d_event d_items d_iwad \
	d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info \
	i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio \
	m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc \
	m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map \
	p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch \
	p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky \
	r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video \
	wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input \
	i_video doomgeneric
OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(DOOM_OBJS))) $(BUILD)/doomgeneric_discitix.o

CFLAGS := -O2 -w $(TARGET_CFLAGS) -I$(SRC) -DNORMALUNIX -D_DEFAULT_SOURCE

all: $(BIN) $(WAD)

$(BIN): $(OBJS) $(CRT0) $(LIBC)
	$(CC) $(TARGET_LDFLAGS) -T $(LDSCRIPT) -o $@ $(CRT0) $(OBJS) -lm -lc

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/doomgeneric_discitix.o: doomgeneric_discitix.c | $(BUILD)
	$(CC) $(CFLAGS) -Wall -Wextra -c $< -o $@

$(WAD): | $(BUILD)
	wget -q -O $@.tmp $(WAD_URL) || { rm -f $@.tmp; exit 1; }
	echo "$(WAD_SHA1)  $@.tmp" | sha1sum -c --quiet
	mv $@.tmp $@

$(BUILD):
	mkdir -p $@

install: all
	@mkdir -p $(DESTDIR)/bin $(DESTDIR)$(WAD_DIR)
	$(INSTALL_PROGRAM) $(BIN) $(DESTDIR)/bin/doom
	install -m644 $(WAD) $(DESTDIR)$(WAD_DIR)/doom1.wad

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
