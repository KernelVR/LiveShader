
	.section ".data"

	.align 4
	.global g_mainFontJp
g_mainFontJp:
	.incbin "../resource/NotoSansJP-Medium.ttf"
g_mainFontJpEnd:

	.global g_mainFontJp_Len
g_mainFontJp_Len:
	.quad g_mainFontJpEnd - g_mainFontJp
