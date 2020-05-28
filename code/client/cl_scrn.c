/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"

qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;
cvar_t		*cl_drawdemoname;
cvar_t		*cl_demodrawheight;
cvar_t		*cl_demodrawwidth;
cvar_t		*cl_demodrawscale;
cvar_t 		*cl_drawspeedo;
cvar_t 		*cl_speedoX;
cvar_t		*cl_speedoY;
cvar_t 		*cl_speedoSize;
cvar_t		*cl_drawangle;
cvar_t		*cl_drawangleX;
cvar_t		*cl_drawangleY;

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float	xscale;
	float	yscale;

#if 0
		// adjust for wide screens
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			*x += 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * 640 / 480 ) );
		}
#endif

	// scale for screen sizes
	xscale = cls.glconfig.vidWidth / 640.0;
	yscale = cls.glconfig.vidHeight / 480.0;
	if ( x ) {
		*x *= xscale;
	}
	if ( y ) {
		*y *= yscale;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -size ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size;
	ah = size;
	SCR_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -smallchar_height ) {
		return;
	}

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallString
** small string are drawn at native screen resolution
*/
void SCR_DrawSmallString( int x, int y, const char *s, int len ) {
	int row, col, ch, i;
	float frow, fcol;
	float size;

	if ( y < -smallchar_height ) {
		return;
	}

	size = 0.0625;

	for ( i = 0; i < len; i++ ) {
		ch = *s++ & 255;
		row = ch>>4;
		col = ch&15;

		frow = row*0.0625;
		fcol = col*0.0625;

		re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
						   fcol, frow, fcol + size, frow + size, 
						   cls.charSetShader );

		x += smallchar_width;
	}
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0.0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( !noColorEscape && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx+2, y+2, size, *s );
		xx += size;
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size;
		s++;
	}
	re.SetColor( NULL );
}


/*
==================
SCR_DrawBigString
==================
*/
void SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse, noColorEscape );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += smallchar_width;
		s++;
	}
	re.SetColor( NULL );
}


/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
** SCR_GetBigStringWidth
*/ 
int SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * BIGCHAR_WIDTH;
}


/*
** SCR_FontWidth
*/
int SCR_FontWidth(const char *text, float scale) {
	if (!cls.fontFont)
		return -1;

	int		count, len;
	float		out;
	glyphInfo_t	*glyph;
	float		useScale;
	const char	*s = text;
	fontInfo_t	*font = &cls.font;


	useScale = scale * font->glyphScale;
	out = 0;
	if (text) {
		len = strlen(text);
		count = 0;

		while (s && *s && count < len) {
			if (Q_IsColorString(s)) {
				s += 2;
				continue;
			}

			glyph = &font->glyphs[(int)*s];
			out  += glyph->xSkip;
			s++;
			count++;
		}
	}

	return out * useScale;
}


/*
** SCR_DrawFontChar
*/

void SCR_DrawFontChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
	if (!cls.fontFont)
		return;

	float  w, h;

	w = width * scale;
	h = height * scale;

 	SCR_AdjustFrom640(&x, &y, &w, &h);
	re.DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}


/*
** SCR_DrawFontText
 */
void SCR_DrawFontText(float x, float y, float scale, vec4_t color, const char *text, int style) {
	if (!cls.fontFont)
		return;

	int		len, count;
	vec4_t		newColor;
	vec4_t		black = {0.0f, 0.0f, 0.0f, 1.0f};
	vec4_t		grey = { 0.2f, 0.2f, 0.2f, 1.0f };
	glyphInfo_t	*glyph;
	float		useScale;
	fontInfo_t	*font = &cls.font;

	useScale = scale * font->glyphScale;

	if (text) {
		const char*s = text;
		re.SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);

		count = 0;

		while (s && *s && count < len) {
			glyph = &font->glyphs[(int)*s];

			if (Q_IsColorString(s)) {
				memcpy( newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				re.SetColor( newColor );
				s += 2;
				continue;
			}

			float  yadj = useScale * glyph->top;

			if ((style == ITEM_TEXTSTYLE_SHADOWED) || (style == ITEM_TEXTSTYLE_SHADOWEDLESS)) {
				black[3] = newColor[3];

				if (style == ITEM_TEXTSTYLE_SHADOWEDLESS)
					black[3] *= 0.7;

				if (newColor[0] == 0.0f && newColor[1] == 0.0f && newColor[2] == 0.0f) {
					grey[3] = black[3];
					re.SetColor(grey);
				} else {
				  re.SetColor(black);
				}

				SCR_DrawFontChar(x + 1, y - yadj + 1,
						 glyph->imageWidth,
						 glyph->imageHeight,
						 useScale,
						 glyph->s,
						 glyph->t,
						 glyph->s2,
						 glyph->t2,
						 glyph->glyph);

				colorBlack[3] = 1.0;
				re.SetColor(newColor);
			}

			SCR_DrawFontChar(x, y - yadj,
					 glyph->imageWidth,
					 glyph->imageHeight,
					 useScale,
					 glyph->s,
					 glyph->t,
					 glyph->s2,
					 glyph->t2,
					 glyph->glyph);
			x += (glyph->xSkip * useScale);
			s++;
			count++;
		}
		re.SetColor(NULL);
	}
}

//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
void SCR_DrawDemoRecording( void ) {
	char	string[sizeof(clc.recordNameShort)+32];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clc.recordfile );

	if( cl_drawdemoname->integer ) {
		sprintf( string, "RECORDING %s: %ik", clc.recordNameShort, pos / 1024 );
	} else {
		sprintf( string, "RECORDING: %ik", pos / 1024 );
	}


	SCR_DrawStringExt( cl_demodrawwidth->integer, cl_demodrawheight->integer, cl_demodrawscale->integer, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}

static void SCR_DrawSpeedo( void )
{
	int speed;
	char string[64];

	if( !cl_drawspeedo->integer )
		return;

	speed = (int) sqrt(pow(cl.snap.ps.velocity[PITCH], 2) + pow(cl.snap.ps.velocity[YAW], 2));

	sprintf( string, "Speed: %d ups", speed );

	SCR_DrawStringExt( cl_speedoX->integer, cl_speedoY->integer, cl_speedoSize->integer, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}

// Draws viewangle relative to movedir
static void SCR_DrawAngle( void )
{
	float yaw, dot, length;
	int angle;
	vec3_t v1, v2;
	char string[64];

	if( !cl_drawangle->integer ) {
		return;
	}

	angle = 0;

	VectorCopy(cl.snap.ps.velocity, v1);
	v1[2] = 0.0f;
	length = VectorLength(v1);

	if( length ) {
		yaw = cl.snap.ps.viewangles[YAW];
		yaw *= M_PI/180.f;

		v2[0] = cos(yaw);
		v2[1] = sin(yaw);
		v2[2] = 0.0f;

		VectorNormalize(v1);
		dot = DotProduct(v1, v2);
		angle = (int) (acosf(dot) * 180.0f/M_PI);
	}

	sprintf(string, "Angle: %d", angle);

	SCR_DrawStringExt( cl_drawangleX->integer, cl_drawangleY->integer, cl_speedoSize->integer, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}

#ifdef USE_VOIP
/*
=================
SCR_DrawVoipMeter
=================
*/
void SCR_DrawVoipMeter( void ) {
	char	buffer[16];
	char	string[256];
	int limit, i;

	if (!cl_voipShowMeter->integer)
		return;  // player doesn't want to show meter at all.
	else if (!cl_voipSend->integer)
		return;  // not recording at the moment.
	else if (clc.state != CA_ACTIVE)
		return;  // not connected to a server.
	else if (!clc.voipEnabled)
		return;  // server doesn't support VoIP.
	else if (clc.demoplaying)
		return;  // playing back a demo.
	else if (!cl_voip->integer)
		return;  // client has VoIP support disabled.

	limit = (int) (clc.voipPower * 10.0f);
	if (limit > 10)
		limit = 10;

	for (i = 0; i < limit; i++)
		buffer[i] = '*';
	while (i < 10)
		buffer[i++] = ' ';
	buffer[i] = '\0';

	sprintf( string, "VoIP: [%s]", buffer );
	SCR_DrawStringExt( 320 - strlen( string ) * 4, 10, 8, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}
#endif


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

static	int			current;
static	float		values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value)
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}


/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, 
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (ARRAY_LEN(values)+current-1-(a % ARRAY_LEN(values))) % ARRAY_LEN(values);
		v = values[i];
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);
	cl_drawdemoname = Cvar_Get("cl_drawdemoname", "0", CVAR_ARCHIVE_ND);
	cl_demodrawheight = Cvar_Get("cl_demodrawheight", "240", CVAR_ARCHIVE_ND);
	cl_demodrawwidth = Cvar_Get("cl_demodrawwidth", "1", CVAR_ARCHIVE_ND);
	cl_demodrawscale = Cvar_Get("cl_demodrawscale", "8", CVAR_ARCHIVE_ND);
	cl_drawspeedo = Cvar_Get("cl_drawspeedo", "0", CVAR_ARCHIVE_ND);
	cl_speedoX = Cvar_Get("cl_speedoX", "250", CVAR_ARCHIVE_ND);
	cl_speedoY = Cvar_Get("cl_speedoY", "240", CVAR_ARCHIVE_ND);
	cl_speedoSize = Cvar_Get("cl_speedoSize", "6", CVAR_ARCHIVE_ND);
	cl_drawangle = Cvar_Get("cl_drawangle", "0", CVAR_ARCHIVE_ND);
	cl_drawangleX = Cvar_Get("cl_drawangleX", "250", CVAR_ARCHIVE_ND);
	cl_drawangleY = Cvar_Get("cl_drawangleY", "210", CVAR_ARCHIVE_ND);

	scr_initialized = qtrue;
}


//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	qboolean uiFullscreen;

	re.BeginFrame( stereoFrame );

	uiFullscreen = (uivm && VM_Call( uivm, 0, UI_IS_FULLSCREEN ));

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( uiFullscreen || cls.state < CA_LOADING ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
			re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if ( uivm && !uiFullscreen ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			// refresh to update the time
			VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
			VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qfalse );
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			if ( cgvm ) {
				CL_CGameRendering( stereoFrame );
			}
			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			// refresh to update the time
			VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
			VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qtrue );
			break;
		case CA_ACTIVE:
			// always supply STEREO_CENTER as vieworg offset is now done by the engine.
			CL_CGameRendering( stereoFrame );
			SCR_DrawDemoRecording();
			SCR_DrawSpeedo();
			SCR_DrawAngle();
#ifdef USE_VOIP
			SCR_DrawVoipMeter();
#endif
			break;
		}
	}

	// the menu draws next
	if ( Key_GetCatcher( ) & KEYCATCH_UI && uivm ) {
		VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
	}

	// console draws next
	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph ();
	}
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int recursive;
	static int framecount;
	static int next_frametime;

	if ( !scr_initialized )
		return; // not initialized yet

	if ( framecount == cls.framecount ) {
		int ms = Sys_Milliseconds();
		if ( next_frametime && ms - next_frametime < 0 ) {
			re.ThrottleBackend();
		} else {
			next_frametime = ms + 16; // limit to 60 FPS
		}
	} else {
		next_frametime = 0;
		framecount = cls.framecount;
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if ( uivm )
	{
		// XXX
		int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
		// if running in stereo, we need to draw the frame twice
		if ( cls.glconfig.stereoEnabled || in_anaglyphMode) {
			SCR_DrawScreenField( STEREO_LEFT );
			SCR_DrawScreenField( STEREO_RIGHT );
		} else {
			SCR_DrawScreenField( STEREO_CENTER );
		}

		if ( com_speeds->integer ) {
			re.EndFrame( &time_frontend, &time_backend );
		} else {
			re.EndFrame( NULL, NULL );
		}
	}

	recursive = 0;
}
