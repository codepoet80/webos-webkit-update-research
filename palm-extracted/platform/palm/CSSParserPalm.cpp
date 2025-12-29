#include "config.h"
#include "CSSParser.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSStyleSheet.h"
#include "CSSStyleRule.h"
#include "CSSValueKeywords.h"


#include "webkitpalmsettings.h"

extern int cssyyparse(void* parser);

namespace WebCore {

void CSSParser::setupParser(const char* prefix, const String& string, const char* suffix)
{
    int length = string.length() + strlen(prefix) + strlen(suffix) + 2;

    if( m_data ) {
        fastFree(m_data);
        m_data = 0;
    }

    if( PalmBrowserSettings()->enableCssOptimizations ) {
        UChar* dataPtr = 0;

        if (length <= kStackDataLen)
            dataPtr = m_stackData;
        else {
            m_data = static_cast<UChar*>(fastMalloc(length * sizeof(UChar)));
            dataPtr = m_data;
        }

        for (unsigned i = 0; i < strlen(prefix); i++)
            dataPtr[i] = prefix[i];

        memcpy(dataPtr + strlen(prefix), string.characters(), string.length() * sizeof(UChar));

        unsigned start = strlen(prefix) + string.length();
        unsigned end = start + strlen(suffix);
        for (unsigned i = start; i < end; i++)
            dataPtr[i] = suffix[i - start];

        dataPtr[length - 1] = 0;
        dataPtr[length - 2] = 0;

        yy_hold_char = 0;
        yyleng = 0;
        yytext = yy_c_buf_p = dataPtr;
        yy_hold_char = *yy_c_buf_p;
    }
    else
    {
        m_data = static_cast<UChar*>(fastMalloc(length * sizeof(UChar)));
        for (unsigned i = 0; i < strlen(prefix); i++)
            m_data[i] = prefix[i];

        memcpy(m_data + strlen(prefix), string.characters(), string.length() * sizeof(UChar));

        unsigned start = strlen(prefix) + string.length();
        unsigned end = start + strlen(suffix);
        for (unsigned i = start; i < end; i++)
            m_data[i] = suffix[i - start];

        m_data[length - 1] = 0;
        m_data[length - 2] = 0;

        yy_hold_char = 0;
        yyleng = 0;
        yytext = yy_c_buf_p = m_data;
        yy_hold_char = *yy_c_buf_p;
    }
    resetRuleBodyMarks();
}

PassRefPtr<CSSRule> CSSParser::parseRule(CSSStyleSheet* sheet, const String& string)
{
    m_styleSheet = sheet;
    m_allowNamespaceDeclarations = false;

    if( PalmBrowserSettings()->enableCssOptimizations )
    {
        // Optimize often-occuring patterns in the Mojo framework.
        if( string == "head{}" )
        {
            CSSSelector* fs = createFloatingSelector();
            fs->m_tag = QualifiedName(nullAtom, ("head"), m_defaultNamespace);

            Vector<CSSSelector*> selectors;
            selectors.append(fs);
            m_rule = createStyleRule(&selectors);
            return m_rule.release();
        }
        else if( string == "div[x-mojo-element]{}" )
        {
            //[
            // specifier
            CSSSelector* mojosel = createFloatingSelector();
            mojosel->setAttribute( QualifiedName(nullAtom, ("x-mojo-element"), nullAtom) );
            mojosel->m_match = CSSSelector::Set;
            mojosel->m_tag = QualifiedName(nullAtom, ("div"), m_defaultNamespace);
            //]

            // '{'
            Vector<CSSSelector*> selectors;
            selectors.append(mojosel);
            m_rule = createStyleRule(&selectors);
            // '}'

            return m_rule.release();
        }
    }

    setupParser("@-webkit-rule{", string, "} ");
    cssyyparse(this);
    return m_rule.release();
}

bool CSSParser::parseValue(CSSMutableStyleDeclaration* declaration, int id, const String& string, bool important)
{
    ASSERT(!declaration->stylesheet() || declaration->stylesheet()->isCSSStyleSheet());
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    m_id = id;
    m_important = important;

    // Optimize often-occuring patterns in the Mojo framework.
    // and hand-parse numeric values.

    const UChar* str =  string.characters();
    const UChar* end = str + string.length();
    unsigned int len = string.length();

    const unsigned int kMaxNumberChars = 64;

    if( PalmBrowserSettings()->enableCssOptimizations )
    {
        //printf("parseValue [%s]\n", string.utf8().data() );

        if( (len > 3) && str[0]=='n' && str[1]=='o' && str[2]=='n' && str[3]=='e') {
            // none
            CSSParserValueList* vv = createFloatingValueList( );// add to m_floatingValueLists
            CSSParserValue v;
            v.id = CSSValueNone;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            vv->addValue( sinkFloatingValue(v) );
            m_valueList = sinkFloatingValueList( vv ); // remove it from m_floatingValueLists
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='h' && str[1]=='i' && str[2]=='d' && str[3]=='d') {
            // hidden
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueHidden;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='b' && str[1]=='l' && str[2]=='o' && str[3]=='c') {
            // block
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueBlock;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='v' && str[1]=='i' && str[2]=='s' && str[3]=='i') {
            // visible
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueVisible;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='r' && str[1]=='e' && str[2]=='l' && str[3]=='a') {
            // relative
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueRelative;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='a' && str[1]=='b' && str[2]=='s' && str[3]=='o') {
            // absolute
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueAbsolute;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 3) && str[0]=='a' && str[1]=='u' && str[2]=='t' && str[3]=='o') {
            // auto
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueAuto;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if( (len > 13) && ( str[0]=='-' && str[1]=='w' && str[2]=='e' && str[3]=='b'
                && str[8]=='p' && str[13]=='o' ) ) {
            // -webkit-palm-overflow
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);
            CSSParserValue v;
            v.id=CSSValueWebkitPalmOverflow;
            v.unit = CSSPrimitiveValue::CSS_IDENT;
            m_valueList->addValue( v);
            int oldpp = m_numParsedProperties;
            if( !parseValue(m_id,m_important) )
                rollbackLastProperties(m_numParsedProperties-oldpp);
            delete m_valueList;
            m_valueList = 0;
        }
        else if ((((len > 1) && (str[0] == '-' && ( str[1] >= 0x0030 && str[1] <= 0x0039)))
                 || ((len > 0) && (str[0] >= 0x0030 && str[0] <= 0x0039)))
                 && len < kMaxNumberChars) {

            //printf("   (number) parseValue [%s]\n", string.utf8().data() );

            static char _cbuf[kMaxNumberChars+1];
            CSSParserValueList* vv = createFloatingValueList( );
            m_valueList = sinkFloatingValueList(vv);

            // if format of css value is not recognized (supported for optimization), we bail out
            bool bailOut = false;

            while( str < end ) {

                CSSParserValue v;
                bool isFloat=false;

                int i=1;
                _cbuf[0] = (char)*str;

                // accumulate the number
                str++;
                while( str != end && ( *str >= 0x0030 && *str <= 0x0039 || *str=='.' ) ) {
                    if( *str == '.' )
                        isFloat = true;
                    _cbuf[i++] = (char)*str;
                    str++;
                }
                _cbuf[i++]=0;
                //printf( "_cbuf -> [%s]\n", _cbuf );

                // Figure out the unit (tokenizer.flex)
                v.unit=CSSPrimitiveValue::CSS_NUMBER;
                if( str != end ) {
                    if ( *str == '%' ) {
                        v.unit=CSSPrimitiveValue::CSS_PERCENTAGE;
                        v.isInt=false;
                    }
                    else if( (str+2) <= end ) {
                        if( *str == 'p' && str[1] == 'x' )
                            v.unit=CSSPrimitiveValue::CSS_PX;
                        else if ( *str == '%' ) {
                            v.unit=CSSPrimitiveValue::CSS_PERCENTAGE;
                            v.isInt=false;
                        }
                        else if ( *str == 'e' && str[1] == 'm' )
                            v.unit=CSSPrimitiveValue::CSS_EMS;
                        else if ( *str == 'e' && str[1] == 'x' )
                            v.unit=CSSPrimitiveValue::CSS_EXS;
                        else if ( *str == 'p' && str[1] == 't' )
                            v.unit=CSSPrimitiveValue::CSS_PT;
                        else if ( *str == 'p' && str[1] == 'c' )
                            v.unit=CSSPrimitiveValue::CSS_PC;
                        else
                            bailOut = true;
                    }
                    else
                        bailOut = true;
                }

                if (bailOut)
                    break;

                // advance over "rest" of qualifier until EOS or whitespace.
                while( str != end && ( *str != 0x0a && *str != 0x0d && *str != '\t' && *str != 0x20 ) )
                    str++;

                // 1. load it into valueList
                if( isFloat ) {
                    v.id=0;
                    v.isInt=false;
                    v.fValue=atof(_cbuf);
                    m_valueList->addValue( v);
                    //printf("flt->%g\n",v.fValue);
                }
                else {
                    // we may be able to skip atoi in a commonly seen case
                    v.id=0;
                    if( _cbuf[0] == '0' && !_cbuf[1] )
                        v.fValue = 0;
                    else
                        v.fValue=(double)atoi(_cbuf);
                    v.isInt=true;
                    m_valueList->addValue( v);
                    //printf("int->%g\n",v.fValue);
                }

                if( str != end ) {
                    // there is another token left. Advance over whitespace to start of it.
                    while( str != end && (*str == 0x20 ||  *str == 0x0a ||  *str == 0x0d || *str == '\t')  )
                        str++;
                }
                else
                    break;
            }

            if (bailOut) {
                m_id = id;
                m_important = important;
                //printf("Bail out parseValue [%s]\n", string.utf8().data() );
                setupParser("@-webkit-value{", string, "} ");
                cssyyparse(this);
            }
            else {
                int oldpp = m_numParsedProperties;
                if (!parseValue(m_id, m_important))
                    rollbackLastProperties(m_numParsedProperties - oldpp);
                delete m_valueList;
                m_valueList = 0;

                m_rule = 0;

                bool ok = false;
                if (m_numParsedProperties) {
                    ok = true;
                    declaration->addParsedProperties(m_parsedProperties, m_numParsedProperties);
                    clearProperties();
                }
                return ok;
            }
        }
        else {
            m_id = id;
            m_important = important;
            //printf("\nparseValue [%s] =_id=%d\n", string.utf8().data(),_id );
            setupParser("@-webkit-value{", string, "} ");
            cssyyparse(this);
        }
    }
    else {
        //printf("\nparseValue [%s] =_id=%d\n", string.utf8().data(),_id );
        setupParser("@-webkit-value{", string, "} ");
        cssyyparse(this);
    }

    m_rule = 0;

    bool ok = false;
    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();
    if (m_numParsedProperties) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties, m_numParsedProperties);
        clearProperties();
    }

    return ok;
}

}
