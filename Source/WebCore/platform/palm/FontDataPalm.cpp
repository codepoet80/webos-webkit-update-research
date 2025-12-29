

#include "config.h"
#include "FontCache.h"
#include "Font.h"
#include "FontData.h"

#include "FloatRect.h"
#include "FontDescription.h"
#include "GlyphBuffer.h"
#include "MGContext.h"
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <wtf/MathExtras.h>


namespace WebCore {

void FontData::platformInit()
{
	if( m_font.m_font )
	{
		m_ascent = m_font.m_font->ascent();
		m_descent = m_font.m_font->descent();
		m_lineSpacing = m_font.m_font->lineheight();
		m_xHeight = m_font.m_font->xheight();
		m_spaceWidth =  m_font.m_font->spacewidth();
		m_lineGap = m_lineSpacing - m_ascent - m_descent;
	}
	else
	{
		fprintf( stderr, "ERROR : You must have bitstream-vera fonts installed: 'sudo apt-get install ttf-bitstream-vera'\n" );
	}
}

void FontData::platformDestroy()
{
	if( m_font.m_font )
		m_font.m_font->release();
	
	delete m_smallCapsFontData;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const 
{
   if (!m_smallCapsFontData) {
        FontDescription desc = FontDescription(fontDescription);
        desc.setComputedSize(0.70f*fontDescription.computedSize());
        const FontPlatformData* pdata = new FontPlatformData(desc, desc.family().family());
        m_smallCapsFontData = new FontData(*pdata);
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
	for (unsigned i = 0; i < length; i++) {
		if( !characters[i] )
			return false;
		
		//printf( "c=$%04x  %ld\n", characters[i], characters[i] );
		//if( 9660 == characters[i] )
		//	return false;
		
		// TODO : we need a containscharcode() function in minigraphics for this.
		
		//if( characters
		//if (FcFreeTypeCharIndex(face, characters[i]) == 0) {
		//    cairo_ft_scaled_font_unlock_face(m_font.m_scaledFont);
		//    return false;
		// }
	}
	/*
    FT_Face face = cairo_ft_scaled_font_lock_face(m_font.m_scaledFont);

    if (!face)
        return false;

    for (unsigned i = 0; i < length; i++) {
        if (FcFreeTypeCharIndex(face, characters[i]) == 0) {
            cairo_ft_scaled_font_unlock_face(m_font.m_scaledFont);
            return false;
        }
    }

    cairo_ft_scaled_font_unlock_face(m_font.m_scaledFont);

    return true;
	*/
	return true;
}

void FontData::determinePitch()
{
    m_treatAsFixedPitch = m_font.isFixedPitch();
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
	return (float) m_font.m_font->width( glyph ); 
}

// Note: the argument is supposed to be a context in which to select the font INTO.
void FontData::setFont(CMgContext* cr) const
{
    ASSERT(cr);
    m_font.setFont(cr);
}

}
