// license:BSD-3-Clause
// copyright-holders:hap
// thanks-to:Sean Riddle
/*******************************************************************************

LJN Video Art

It's a toy for drawing/coloring pictures on the tv, not a video game console.
Picture libraries were available on separate cartridges.

On the splash screen, press CLEAR to start drawing (no need to wait half a minute).
To change the background color, choose one from the color slider and press CLEAR.
Drawing with the same color as the picture outline is not allowed.

Hardware notes:
- EF6805R2P @ 3.57Mhz (14.318MHz XTAL)
- EF9367P @ 1.507MHz, 128*208 resolution (internally 512*208), 16 colors
- TSGB01019ACP unknown 48-pin DIP, interfaces with EF9367P and DRAM
- 2*D41416C-15 (16Kbit*4) DRAM
- 36-pin cartridge slot, 8KB or 16KB ROM
- DB9 joystick port, no known peripherals other than the default analog joystick
- RF NTSC video, no sound

TODO:
- gaps in fast pencil drawing when the outline color is 0xf and background color
  is 0x0 (eg. activity cartridge default), it works fine everywhere else
- custom chip command upper bits meaning is unknown
- palette is approximated from photos/videos

*******************************************************************************/

#include "emu.h"

#include "bus/generic/carts.h"
#include "bus/generic/slot.h"
#include "cpu/m6805/m68705.h"
#include "machine/timer.h"
#include "video/ef9365.h"

#include "emupal.h"
#include "screen.h"
#include "softlist_dev.h"


namespace {

class videoart_state : public driver_device
{
public:
	videoart_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_ef9367(*this, "ef9367"),
		m_vram(*this, "vram", 0x8000, ENDIANNESS_LITTLE),
		m_screen(*this, "screen"),
		m_cart(*this, "cartslot"),
		m_inputs(*this, "IN%u", 0),
		m_led(*this, "led")
	{ }

	void videoart(machine_config &config);

protected:
	virtual void machine_start() override;

private:
	required_device<m6805r2_device> m_maincpu;
	required_device<ef9365_device> m_ef9367;
	memory_share_creator<u8> m_vram;
	required_device<screen_device> m_screen;
	required_device<generic_slot_device> m_cart;
	required_ioport_array<3> m_inputs;
	output_finder<> m_led;

	DECLARE_DEVICE_IMAGE_LOAD_MEMBER(cart_load);

	TIMER_DEVICE_CALLBACK_MEMBER(scanline) { m_ef9367->update_scanline(param); }
	u32 screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void palette(palette_device &palette) const;

	void vram_map(address_map &map);
	void vram_w(offs_t offset, u8 data);

	void porta_w(u8 data);
	u8 porta_r();
	void portb_w(u8 data);
	void portc_w(u8 data);
	u8 portd_r();

	u8 m_porta = 0xff;
	u8 m_portb = 0xff;
	u8 m_portc = 0xff;
	u8 m_rdata = 0xff;
	u8 m_romlatch = 0;
	u8 m_ccount = 0;
	u8 m_command = 0;
	u8 m_color = 0;
};



/*******************************************************************************
    Initialization
*******************************************************************************/

void videoart_state::machine_start()
{
	m_led.resolve();

	// register for savestates
	save_item(NAME(m_porta));
	save_item(NAME(m_portb));
	save_item(NAME(m_portc));
	save_item(NAME(m_rdata));
	save_item(NAME(m_romlatch));
	save_item(NAME(m_ccount));
	save_item(NAME(m_command));
	save_item(NAME(m_color));
}

DEVICE_IMAGE_LOAD_MEMBER(videoart_state::cart_load)
{
	u32 size = m_cart->common_get_size("rom");
	m_cart->rom_alloc(size, GENERIC_ROM8_WIDTH, ENDIANNESS_LITTLE);
	m_cart->common_load_rom(m_cart->get_rom_base(), size, "rom");

	return std::make_pair(std::error_condition(), std::string());
}



/*******************************************************************************
    Video
*******************************************************************************/

constexpr rgb_t videoart_colors[] =
{
	{ 0x00, 0x00, 0x00 }, // 2 black
	{ 0x50, 0x20, 0x28 }, // b dark pink
	{ 0x80, 0x80, 0x80 }, // 1 gray
	{ 0xff, 0x78, 0xff }, // c pink

	{ 0x20, 0x18, 0x90 }, // 7 blue
	{ 0x40, 0x10, 0x50 }, // 8 purple
	{ 0x68, 0xa8, 0xff }, // 6 cyan
	{ 0xd8, 0x78, 0xff }, // 9 lilac

	{ 0x00, 0x60, 0x00 }, // 3 dark green
	{ 0x30, 0x28, 0x08 }, // a brown
	{ 0x68, 0xb0, 0x18 }, // 4 lime green
	{ 0xd0, 0x78, 0x20 }, // f orange

	{ 0xff, 0xff, 0xff }, // 0 white
	{ 0x40, 0x10, 0x10 }, // d dark red
	{ 0x48, 0xb0, 0x20 }, // 5 green
	{ 0xe0, 0x60, 0x58 }  // e light red
};

void videoart_state::palette(palette_device &palette) const
{
	// initialize palette (there is no color prom)
	palette.set_pen_colors(0, videoart_colors);
}

u32 videoart_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// width of 512 compressed down to 128
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
		for (int x = cliprect.min_x; x <= cliprect.max_x; x++)
			bitmap.pix(y, x) = m_vram[(y << 7 | x >> 2) & 0x7fff];

	return 0;
}

void videoart_state::vram_w(offs_t offset, u8 data)
{
	u8 low = m_ef9367->get_msl() & 7;
	data = BIT(data, low ^ 7);
	offset = offset << 1 | BIT(low, 2);

	if (data)
		m_vram[offset] = m_color & 0xf;
	else
		m_vram[offset] ^= 0xf;
}

void videoart_state::vram_map(address_map &map)
{
	map(0x0000, 0x3fff).w(FUNC(videoart_state::vram_w)).nopr();
}



/*******************************************************************************
    I/O
*******************************************************************************/

void videoart_state::porta_w(u8 data)
{
	// A0-A7: EF9367 data
	// A0,A1: TSG command
	m_porta = data;
}

u8 videoart_state::porta_r()
{
	u8 data = 0xff;

	// read cartridge data
	if (~m_portb & 0x10)
	{
		u16 offset = m_romlatch << 8 | m_portc;
		data &= m_cart->read_rom(offset);
	}

	// read EF9367 data
	if (~m_portb & 1)
		data &= m_rdata;

	return data;
}

void videoart_state::portb_w(u8 data)
{
	// B0: EF9367 E
	if (~data & m_portb & 1)
	{
		if (m_portc & 0x10)
			m_rdata = m_ef9367->data_r(m_portc & 0xf);
		else
			m_ef9367->data_w(m_portc & 0xf, m_porta);
	}

	// B1: clock ROM address latch
	if (data & ~m_portb & 2)
		m_romlatch = m_portc;

	// B2: custom chip command
	if (~data & m_portb & 4)
	{
		m_command = (m_command << 2) | (m_porta & 3);

		// reset count
		if (~data & 2)
			m_ccount = 0;

		// change color
		if (m_ccount == 3)
			m_color = m_command & 0xf;

		m_ccount++;
	}

	// B3: erase led
	m_led = BIT(~data, 3);

	// B4: ROM _OE
	// B5-B7: input mux
	m_portb = data;
}

void videoart_state::portc_w(u8 data)
{
	// C0-C7: ROM address
	// C0-C3: EF9367 address
	// C4: EF9367 R/W
	m_portc = data;
}

u8 videoart_state::portd_r()
{
	u8 data = 0;

	// D6,D7: multiplexed inputs
	for (int i = 0; i < 3; i++)
		if (!BIT(m_portb, 5 + i))
			data |= m_inputs[i]->read() & 0xc0;

	return ~data;
}



/*******************************************************************************
    Input Ports
*******************************************************************************/

static INPUT_PORTS_START( videoart )
	PORT_START("IN0")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_BUTTON5) PORT_NAME("Page")
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_BUTTON6) PORT_NAME("Clear") // actually 2 buttons

	PORT_START("IN1")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Horizontal")
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Vertical")

	PORT_START("IN2")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Draw")
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_BUTTON4) PORT_NAME("Erase")

	PORT_START("AN0")
	PORT_BIT(0xff, 0x80, IPT_AD_STICK_X) PORT_SENSITIVITY(50) PORT_KEYDELTA(2) PORT_CENTERDELTA(0) PORT_REVERSE PORT_PLAYER(2) PORT_NAME("Color")

	PORT_START("AN1")
	PORT_BIT(0xff, 0x80, IPT_AD_STICK_X) PORT_SENSITIVITY(50) PORT_KEYDELTA(4) PORT_CENTERDELTA(0)

	PORT_START("AN2")
	PORT_BIT(0xff, 0x80, IPT_AD_STICK_Y) PORT_SENSITIVITY(50) PORT_KEYDELTA(4) PORT_CENTERDELTA(0) PORT_REVERSE
INPUT_PORTS_END



/*******************************************************************************
    Machine Configs
*******************************************************************************/

void videoart_state::videoart(machine_config &config)
{
	// basic machine hardware
	M6805R2(config, m_maincpu, 14.318181_MHz_XTAL / 4);
	m_maincpu->porta_w().set(FUNC(videoart_state::porta_w));
	m_maincpu->porta_r().set(FUNC(videoart_state::porta_r));
	m_maincpu->portb_w().set(FUNC(videoart_state::portb_w));
	m_maincpu->portc_w().set(FUNC(videoart_state::portc_w));
	m_maincpu->portd_r().set(FUNC(videoart_state::portd_r));
	m_maincpu->portan_r<0>().set_ioport("AN0");
	m_maincpu->portan_r<1>().set_ioport("AN1");
	m_maincpu->portan_r<2>().set_ioport("AN2");

	// video hardware
	PALETTE(config, "palette", FUNC(videoart_state::palette), 16);

	EF9365(config, m_ef9367, (14.318181_MHz_XTAL * 2) / 19);
	m_ef9367->set_addrmap(0, &videoart_state::vram_map);
	m_ef9367->set_palette_tag("palette"); // unused there
	m_ef9367->set_nb_bitplanes(1);
	m_ef9367->set_display_mode(ef9365_device::DISPLAY_MODE_512x256);
	m_ef9367->irq_handler().set_inputline(m_maincpu, M6805_IRQ_LINE);

	TIMER(config, "scanline").configure_scanline(FUNC(videoart_state::scanline), "screen", 0, 1);

	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_refresh_hz(60);
	m_screen->set_screen_update(FUNC(videoart_state::screen_update));
	m_screen->set_size(512, 256);
	m_screen->set_visarea(0, 512-1, 48, 256-1);
	m_screen->set_palette("palette");

	// cartridge
	GENERIC_CARTSLOT(config, m_cart, generic_linear_slot, "videoart");
	m_cart->set_device_load(FUNC(videoart_state::cart_load));

	SOFTWARE_LIST(config, "cart_list").set_original("videoart");
}



/*******************************************************************************
    ROM Definitions
*******************************************************************************/

ROM_START( videoart )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD("ljd091.u6", 0x0000, 0x1000, CRC(111ad7d4) SHA1(dec751069a6713ec2e033aed5657378ccfcddebb) )
ROM_END

} // anonymous namespace



/*******************************************************************************
    Drivers
*******************************************************************************/

//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT     CLASS           INIT        COMPANY, FULLNAME, FLAGS
SYST( 1987, videoart, 0,      0,      videoart, videoart, videoart_state, empty_init, "LJN Toys", "Video Art", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_COLORS | MACHINE_NO_SOUND_HW )
