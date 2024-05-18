/*
   pl_json.h

   Do this:
        #define PL_JSON_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_JSON_IMPLEMENTATION
   #include "pl_json.h"
*/

// library version
#define PL_JSON_VERSION    "0.2.0"
#define PL_JSON_VERSION_NUM 00200

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] internal structs
// [SECTION] jsmn.h
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_JSON_H
#define PL_JSON_H

#ifndef PL_JSON_MAX_NAME_LENGTH
    #define PL_JSON_MAX_NAME_LENGTH 256
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plJsonObject plJsonObject; // opaque pointer

// enums
typedef int plJsonType; // internal

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// main
bool          pl_load_json               (const char* cPtrJson, plJsonObject* tPtrJsonOut);
void          pl_unload_json             (plJsonObject* tPtrJson);
char*         pl_write_json              (plJsonObject* tPtrJson, char* pcBuffer, uint32_t* puBufferSize);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~reading~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// members
plJsonObject* pl_json_member_by_name     (plJsonObject* tPtrJson, const char* pcName);
plJsonObject* pl_json_member_by_index    (plJsonObject* tPtrJson, uint32_t uIndex);
void          pl_json_member_list        (plJsonObject* tPtrJson, char** cPtrListOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength);
bool          pl_json_member_exist       (plJsonObject* tPtrJson, const char* pcName);

// retrieve and cast values (default used if member isn't present)
int           pl_json_int_member         (plJsonObject* tPtrJson, const char* pcName,      int iDefaultValue);
uint32_t      pl_json_uint_member        (plJsonObject* tPtrJson, const char* pcName, uint32_t uDefaultValue);
float         pl_json_float_member       (plJsonObject* tPtrJson, const char* pcName,    float fDefaultValue);
double        pl_json_double_member      (plJsonObject* tPtrJson, const char* pcName,   double dDefaultValue);
char*         pl_json_string_member      (plJsonObject* tPtrJson, const char* pcName,    char* cPtrDefaultValue, uint32_t uLength);
bool          pl_json_bool_member        (plJsonObject* tPtrJson, const char* pcName,    bool bDefaultValue);
plJsonObject* pl_json_member             (plJsonObject* tPtrJson, const char* pcName);
plJsonObject* pl_json_array_member       (plJsonObject* tPtrJson, const char* pcName, uint32_t* uPtrSizeOut);

// retrieve and cast array values (default used if member isn't present)
void          pl_json_int_array_member   (plJsonObject* tPtrJson, const char* pcName,      int* iPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_uint_array_member  (plJsonObject* tPtrJson, const char* pcName, uint32_t* uPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_float_array_member (plJsonObject* tPtrJson, const char* pcName,    float* fPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_double_array_member(plJsonObject* tPtrJson, const char* pcName,   double* dPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_bool_array_member  (plJsonObject* tPtrJson, const char* pcName,     bool* bPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_string_array_member(plJsonObject* tPtrJson, const char* pcName,    char** cPtrOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength);

// cast values
int           pl_json_as_int             (plJsonObject* tPtrJson);
uint32_t      pl_json_as_uint            (plJsonObject* tPtrJson);
float         pl_json_as_float           (plJsonObject* tPtrJson);
double        pl_json_as_double          (plJsonObject* tPtrJson);
const char*   pl_json_as_string          (plJsonObject* tPtrJson);
bool          pl_json_as_bool            (plJsonObject* tPtrJson);

// cast array values
void          pl_json_as_int_array       (plJsonObject* tPtrJson,      int* iPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_as_uint_array      (plJsonObject* tPtrJson, uint32_t* uPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_as_float_array     (plJsonObject* tPtrJson,    float* fPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_as_double_array    (plJsonObject* tPtrJson,   double* dPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_as_bool_array      (plJsonObject* tPtrJson,     bool* bPtrOut, uint32_t* uPtrSizeOut);
void          pl_json_as_string_array    (plJsonObject* tPtrJson,    char** cPtrOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~writing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void           pl_json_add_int_member    (plJsonObject* tPtrJson, const char* pcName, int iValue);
void           pl_json_add_uint_member   (plJsonObject* tPtrJson, const char* pcName, uint32_t uValue);
void           pl_json_add_float_member  (plJsonObject* tPtrJson, const char* pcName, float fValue);
void           pl_json_add_double_member (plJsonObject* tPtrJson, const char* pcName, double dValue);
void           pl_json_add_bool_member   (plJsonObject* tPtrJson, const char* pcName, bool bValue);
void           pl_json_add_string_member (plJsonObject* tPtrJson, const char* pcName, const char* pcValue);
void           pl_json_add_member        (plJsonObject* tPtrJson, const char* pcName, plJsonObject* ptValue);
void           pl_json_add_member_array  (plJsonObject* tPtrJson, const char* pcName, plJsonObject* ptValues, uint32_t uSize);
void           pl_json_add_int_array     (plJsonObject* tPtrJson, const char* pcName, int* piValues, uint32_t uSize);
void           pl_json_add_uint_array    (plJsonObject* tPtrJson, const char* pcName, uint32_t* puValues, uint32_t uSize);
void           pl_json_add_float_array   (plJsonObject* tPtrJson, const char* pcName, float* pfValues, uint32_t uSize);
void           pl_json_add_double_array  (plJsonObject* tPtrJson, const char* pcName, double* pdValues, uint32_t uSize);
void           pl_json_add_bool_array    (plJsonObject* tPtrJson, const char* pcName, bool* pbValues, uint32_t uSize);
void           pl_json_add_string_array  (plJsonObject* tPtrJson, const char* pcName, char** ppcBuffer, uint32_t uSize);

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plJsonObject
{
    plJsonType    tType;
    uint32_t      uChildCount;
    char          acName[PL_JSON_MAX_NAME_LENGTH]; 
    plJsonObject* sbtChildren;
    uint32_t      uChildrenFound;
    char*         sbcBuffer;
    char**        psbcBuffer;
    
    union
    {
        struct
        {
            uint32_t*    sbuValueOffsets;
            uint32_t*    sbuValueLength;
        };

        struct
        {
            uint32_t    uValueOffset;
            uint32_t    uValueLength;
        };    
    };
    
} plJsonObject;

#endif //PL_JSON_H

#ifdef PL_JSON_IMPLEMENTATION

#ifndef PL_GLTF_EXTENSION_H

//-----------------------------------------------------------------------------
// [SECTION] jsmn.h
//-----------------------------------------------------------------------------

/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

typedef enum 
{
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT    = 1 << 0,
  JSMN_ARRAY     = 1 << 1,
  JSMN_STRING    = 1 << 2,
  JSMN_PRIMITIVE = 1 << 3  // number, boolean (true/false) or null
} jsmntype_t;

enum jsmnerr 
{
  
  JSMN_ERROR_NOMEM = -1, // Not enough tokens were provided
  JSMN_ERROR_INVAL = -2, // Invalid character inside JSON string
  JSMN_ERROR_PART  = -3  // The string is not a full JSON packet, more bytes expected
};

typedef struct jsmntok 
{
  jsmntype_t type;  // type (object, array, string etc.)
  int        start; // start position in JSON data string
  int        end;   // end position in JSON data string
  int        size;
} jsmntok_t;

typedef struct jsmn_parser
{
  unsigned int pos;      // offset in the JSON string
  unsigned int toknext;  // next token to allocate
  int          toksuper; // superior token node, e.g. parent object or array
} jsmn_parser;


JSMN_API void jsmn_init (jsmn_parser* parser);
JSMN_API int  jsmn_parse(jsmn_parser* parser, const char* js, const size_t len, jsmntok_t* tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER

// allocates a fresh unused token from the token pool.
static jsmntok_t*
jsmn_alloc_token(jsmn_parser* parser, jsmntok_t* tokens, const size_t num_tokens) 
{
  jsmntok_t* tok;
  if (parser->toknext >= num_tokens)
    return NULL;
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
  return tok;
}

// Fills token type and boundaries.
static void
jsmn_fill_token(jsmntok_t* token, const jsmntype_t type, const int start, const int end) 
{
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

// Fills next available token with JSON primitive.
static int
jsmn_parse_primitive(jsmn_parser* parser, const char* js, const size_t len, jsmntok_t* tokens, const size_t num_tokens) 
{
  jsmntok_t* token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) 
  {
    switch (js[parser->pos]) 
    {
    /* In strict mode primitive must be followed by "," or "}" or "]" */
    case ':':
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
                   /* to quiet a warning from gcc*/
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) 
    {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }
found:
  if (tokens == NULL) 
  {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == NULL) 
  {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
  parser->pos--;
  return 0;
}

// fills next token with JSON string.
static int 
jsmn_parse_string(jsmn_parser* parser, const char* js, const size_t len, jsmntok_t* tokens, const size_t num_tokens) 
{
  jsmntok_t* token;

  int start = parser->pos;
  
  /* Skip starting quote */
  parser->pos++;
  
  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) 
  {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') 
    {
      if (tokens == NULL)
        return 0;
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) 
      {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) 
    {
      int i;
      parser->pos++;
      switch (js[parser->pos]) 
      {
      /* Allowed escaped symbols */
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      /* Allows escaped symbol \uXXXX */
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) 
        {
          /* If it isn't a hex character we have an error */
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
                (js[parser->pos] >= 97 && js[parser->pos] <= 102)))   /* a-f */
            {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      /* Unexpected symbol */
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

JSMN_API int
jsmn_parse(jsmn_parser* parser, const char* js, const size_t len, jsmntok_t* tokens, const unsigned int num_tokens) 
{
  int r;
  int i;
  jsmntok_t* token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) 
  {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) 
    {
    case '{':
    case '[':
      count++;
      if (tokens == NULL)
        break;
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL)
        return JSMN_ERROR_NOMEM;
      if (parser->toksuper != -1) 
      {
        jsmntok_t *t = &tokens[parser->toksuper];
        t->size++;
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      if (tokens == NULL)
        break;
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
      for (i = parser->toknext - 1; i >= 0; i--) 
      {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) 
        {
          if (token->type != type)
            return JSMN_ERROR_INVAL;
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      /* Error if unmatched closing bracket */
      if (i == -1)
        return JSMN_ERROR_INVAL;
      for (; i >= 0; i--) 
      {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) 
        {
          parser->toksuper = i;
          break;
        }
      }
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
      if (r < 0)
        return r;
      count++;
      if (parser->toksuper != -1 && tokens != NULL)
        tokens[parser->toksuper].size++;
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 &&
          tokens[parser->toksuper].type != JSMN_ARRAY &&
          tokens[parser->toksuper].type != JSMN_OBJECT) 
        {
            for (i = parser->toknext - 1; i >= 0; i--) 
            {
            if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) 
            {
                if (tokens[i].start != -1 && tokens[i].end == -1) 
                {
                parser->toksuper = i;
                break;
                }
            }
            }
        }
      break;

    /* In non-strict mode every unquoted value is a primitive */
    default:
      r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
      if (r < 0)
        return r;
      count++;
      if (parser->toksuper != -1 && tokens != NULL)
        tokens[parser->toksuper].size++;
      break;
    }
  }

  if (tokens != NULL) 
  {
    for (i = parser->toknext - 1; i >= 0; i--) {
      /* Unmatched opened object or array */
      if (tokens[i].start != -1 && tokens[i].end == -1)
        return JSMN_ERROR_PART;
    }
  }

  return count;
}

JSMN_API void
jsmn_init(jsmn_parser* parser)
{
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

#endif

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] includes
// [SECTION] stretchy buffer
// [SECTION] internal api
// [SECTION] internal enums
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#if defined(PL_JSON_ALLOC) && defined(PL_JSON_FREE)
// ok
#elif !defined(PL_JSON_ALLOC) && !defined(PL_JSON_FREE)
// ok
#else
#error "Must define both or none of PL_JSON_ALLOC and PL_JSON_FREE"
#endif

#ifndef PL_JSON_ALLOC
    #include <stdlib.h>
    #define PL_JSON_ALLOC(x) malloc(x)
    #define PL_JSON_FREE(x)  free((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] stretchy buffer
//-----------------------------------------------------------------------------

// based on pl_ds.h version "0.2.0"

#define pl_sb_json_capacity(buf) \
    ((buf) ? pl__sb_json_header((buf))->uCapacity : 0u)

#define pl_sb_json_size(buf) \
    ((buf) ? pl__sb_json_header((buf))->uSize : 0u)

#define pl_sb_json_pop(buf) \
    (buf)[--pl__sb_json_header((buf))->uSize]

#define pl_sb_json_top(buf) \
    ((buf)[pl__sb_json_header((buf))->uSize-1])

#define pl_sb_json_free(buf) \
    if((buf)){ PL_JSON_FREE(pl__sb_json_header(buf));} (buf) = NULL;

#define pl_sb_json_reset(buf) \
    if((buf)){ pl__sb_json_header((buf))->uSize = 0u;}

#define pl_sb_json_push(buf, v) \
    (pl__sb_json_may_grow((buf), sizeof(*(buf)), 1, 8), (buf)[pl__sb_json_header((buf))->uSize++] = (v))

#define pl_sb_json_reserve(buf, n) \
    (pl__sb_json_may_grow((buf), sizeof(*(buf)), (n), (n)))

#define pl_sb_json_resize(buf, n) \
    (pl__sb_json_may_grow((buf), sizeof(*(buf)), (n), (n)), pl__sb_json_header((buf))->uSize = (n))

#define pl_sb_json_add_n(buf, n) \
    (pl__sb_json_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (pl__sb_json_header(buf)->uSize += (n), pl__sb_json_header(buf)->uSize - (n)) : pl_sb_json_size(buf))

#define pl_sb_json_sprintf(buf, pcFormat, ...) \
    pl__sb_json_sprintf(&(buf), (pcFormat), __VA_ARGS__)

#define pl__sb_json_header(buf) ((plSbJsonHeader_*)(((char*)(buf)) - sizeof(plSbJsonHeader_)))
#define pl__sb_json_may_grow(buf, s, n, m) pl__sb_json_may_grow_((void**)&(buf), (s), (n), (m))

typedef struct
{
    uint32_t uSize;
    uint32_t uCapacity;
} plSbJsonHeader_;

static void
pl__sb_json_grow(void** ptrBuffer, size_t szElementSize, size_t szNewItems)
{

    plSbJsonHeader_* ptOldHeader = pl__sb_json_header(*ptrBuffer);

    plSbJsonHeader_* ptNewHeader = (plSbJsonHeader_*)PL_JSON_ALLOC((ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbJsonHeader_));
    memset(ptNewHeader, 0, (ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbJsonHeader_));
    if(ptNewHeader)
    {
        ptNewHeader->uSize = ptOldHeader->uSize;
        ptNewHeader->uCapacity = ptOldHeader->uCapacity + (uint32_t)szNewItems;
        memcpy(&ptNewHeader[1], *ptrBuffer, ptOldHeader->uSize * szElementSize);
        PL_JSON_FREE(ptOldHeader);
        *ptrBuffer = &ptNewHeader[1];
    }
}

static void
pl__sb_json_may_grow_(void** ptrBuffer, size_t szElementSize, size_t szNewItems, size_t szMinCapacity)
{
    if(*ptrBuffer)
    {   
        plSbJsonHeader_* ptOriginalHeader = pl__sb_json_header(*ptrBuffer);
        if(ptOriginalHeader->uSize + szNewItems > ptOriginalHeader->uCapacity)
        {
            pl__sb_json_grow(ptrBuffer, szElementSize, szNewItems);
        }
    }
    else // first run
    {
        plSbJsonHeader_* ptHeader = (plSbJsonHeader_*)PL_JSON_ALLOC(szMinCapacity * szElementSize + sizeof(plSbJsonHeader_));
        memset(ptHeader, 0, szMinCapacity * szElementSize + sizeof(plSbJsonHeader_));
        if(ptHeader)
        {
            *ptrBuffer = &ptHeader[1]; 
            ptHeader->uSize = 0u;
            ptHeader->uCapacity = (uint32_t)szMinCapacity;
        }
    }     
}

static void
pl__sb_json_vsprintf(char** ppcBuffer, const char* pcFormat, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    int32_t n = vsnprintf(NULL, 0, pcFormat, args2);
    va_end(args2);
    uint32_t an = pl_sb_json_size(*ppcBuffer);
    pl_sb_json_resize(*ppcBuffer, an + n + 1);
    vsnprintf(*ppcBuffer + an, n + 1, pcFormat, args);
}

static void
pl__sb_json_sprintf(char** ppcBuffer, const char* pcFormat, ...)
{
    va_list args;
    va_start(args, pcFormat);
    pl__sb_json_vsprintf(ppcBuffer, pcFormat, args);
    va_end(args);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plJsonType pl__get_json_token_object_type(const char* cPtrJson, jsmntok_t* tPtrToken);
static void       pl__write_json_object(plJsonObject* ptJson, char* pcBuffer, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth);
static void       pl__check_json_object(plJsonObject* ptJson, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth);

//-----------------------------------------------------------------------------
// [SECTION] internal enums
//-----------------------------------------------------------------------------

enum plJsonType_
{
	PL_JSON_TYPE_UNSPECIFIED,
	PL_JSON_TYPE_STRING,
	PL_JSON_TYPE_ARRAY,
	PL_JSON_TYPE_NUMBER,
	PL_JSON_TYPE_BOOL,
	PL_JSON_TYPE_OBJECT,
	PL_JSON_TYPE_NULL,
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_load_json(const char* cPtrJson, plJsonObject* tPtrJsonOut)
{
    jsmn_parser tP = {0};
    jsmntok_t* sbtTokens = NULL;
    pl_sb_json_resize(sbtTokens, 512);

    jsmn_init(&tP);

    int iResult = 0;
    while(true)
    {
        iResult = jsmn_parse(&tP, cPtrJson, strlen(cPtrJson), sbtTokens, pl_sb_json_size(sbtTokens));

        if(iResult == JSMN_ERROR_INVAL)
        {
            PL_ASSERT(false);
        }
        else if(iResult == JSMN_ERROR_NOMEM)
        {
            pl_sb_json_add_n(sbtTokens, 256);
        }
        else if(iResult == JSMN_ERROR_PART)
        {
            PL_ASSERT(false);
        }
        else
        {
            break;
        }
    }

    uint32_t uLayer = 0;
    uint32_t uCurrentTokenIndex = 0;
    plJsonObject** sbtObjectStack = NULL;
    tPtrJsonOut->tType = PL_JSON_TYPE_OBJECT;
    pl_sb_json_reserve(tPtrJsonOut->sbcBuffer, strlen(cPtrJson));
    tPtrJsonOut->psbcBuffer = &tPtrJsonOut->sbcBuffer;
    tPtrJsonOut->uChildCount = sbtTokens[uCurrentTokenIndex].size;
    strcpy(tPtrJsonOut->acName, "ROOT");
    pl_sb_json_reserve(tPtrJsonOut->sbtChildren, sbtTokens[uCurrentTokenIndex].size);
    pl_sb_json_push(sbtObjectStack, tPtrJsonOut);
    while(uCurrentTokenIndex < (uint32_t)iResult)
    {

        if(pl_sb_json_top(sbtObjectStack)->uChildrenFound == pl_sb_json_top(sbtObjectStack)->uChildCount)
            pl_sb_json_pop(sbtObjectStack);
        else
        {
            
            plJsonObject* tPtrParentObject = pl_sb_json_top(sbtObjectStack);

            jsmntok_t* tPtrCurrentToken = &sbtTokens[uCurrentTokenIndex];
            jsmntok_t* tPtrNextToken = &sbtTokens[uCurrentTokenIndex + 1];

            switch (tPtrCurrentToken->type)
            {

            // value
            case JSMN_PRIMITIVE:
                if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    const uint32_t uBufferLocation = pl_sb_json_size(tPtrJsonOut->sbcBuffer);
                    pl_sb_json_resize(tPtrJsonOut->sbcBuffer, uBufferLocation + tPtrCurrentToken->end - tPtrCurrentToken->start + 1);
                    memcpy(&tPtrJsonOut->sbcBuffer[uBufferLocation], &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                    pl_sb_json_push(tPtrParentObject->sbuValueOffsets, uBufferLocation);
                    pl_sb_json_push(tPtrParentObject->sbuValueLength, tPtrCurrentToken->end - tPtrCurrentToken->start);
                    tPtrParentObject->uChildrenFound++;
                }
                else
                {
                    const uint32_t uBufferLocation = pl_sb_json_size(tPtrJsonOut->sbcBuffer);
                    pl_sb_json_resize(tPtrJsonOut->sbcBuffer, uBufferLocation + tPtrCurrentToken->end - tPtrCurrentToken->start + 1);
                    memcpy(&tPtrJsonOut->sbcBuffer[uBufferLocation], &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                    tPtrParentObject->uValueOffset = uBufferLocation;
                    tPtrParentObject->uValueLength = tPtrCurrentToken->end - tPtrCurrentToken->start;
                    tPtrParentObject->uChildrenFound++;
                    pl_sb_json_pop(sbtObjectStack);
                }
                break;

            case JSMN_STRING:
            {
                // value
                if(tPtrCurrentToken->size == 0)
                {
                    if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                    {
                        const uint32_t uBufferLocation = pl_sb_json_size(tPtrJsonOut->sbcBuffer);
                        pl_sb_json_resize(tPtrJsonOut->sbcBuffer, uBufferLocation + tPtrCurrentToken->end - tPtrCurrentToken->start + 1);
                        memcpy(&tPtrJsonOut->sbcBuffer[uBufferLocation], &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                        pl_sb_json_push(tPtrParentObject->sbuValueOffsets, uBufferLocation);
                        pl_sb_json_push(tPtrParentObject->sbuValueLength, tPtrCurrentToken->end - tPtrCurrentToken->start);
                        tPtrParentObject->uChildrenFound++;
                    }
                    else
                    {
                        const uint32_t uBufferLocation = pl_sb_json_size(tPtrJsonOut->sbcBuffer);
                        pl_sb_json_resize(tPtrJsonOut->sbcBuffer, uBufferLocation + tPtrCurrentToken->end - tPtrCurrentToken->start + 1);
                        memcpy(&tPtrJsonOut->sbcBuffer[uBufferLocation], &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                        tPtrParentObject->uValueOffset = uBufferLocation;
                        tPtrParentObject->uValueLength = tPtrCurrentToken->end - tPtrCurrentToken->start;
                        tPtrParentObject->uChildrenFound++;
                        pl_sb_json_pop(sbtObjectStack);
                    }
                }

                // key
                else
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(cPtrJson, tPtrNextToken),
                        (uint32_t)tPtrNextToken->size
                    };
                    tNewJsonObject.psbcBuffer = &tPtrJsonOut->sbcBuffer;
                    if(tNewJsonObject.uChildCount == 0)
                    {
                        tNewJsonObject.uChildrenFound--;
                    }
                    tPtrParentObject->uChildrenFound++;
                    strncpy(tNewJsonObject.acName, &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                    pl_sb_json_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrNextToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueOffsets, tPtrNextToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrNextToken->size);
                    pl_sb_json_push(sbtObjectStack, &pl_sb_json_top(tPtrParentObject->sbtChildren));

                    if(tNewJsonObject.tType == PL_JSON_TYPE_ARRAY)
                    {
                        uCurrentTokenIndex++;
                    }
                }
                break;
            }

            case JSMN_OBJECT:
            {
                if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(cPtrJson, tPtrCurrentToken),
                        (uint32_t)tPtrCurrentToken->size
                    };
                    tNewJsonObject.psbcBuffer = &tPtrJsonOut->sbcBuffer;
                    strcpy(tNewJsonObject.acName, "UNNAMED OBJECT");
                    pl_sb_json_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrCurrentToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueOffsets, tPtrCurrentToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrCurrentToken->size);
                    pl_sb_json_push(sbtObjectStack, &pl_sb_json_top(tPtrParentObject->sbtChildren));
                    tPtrParentObject->uChildrenFound++;
                }
                else if(tPtrParentObject->tType == PL_JSON_TYPE_OBJECT)
                {
                    // combining key/pair
                    // tPtrParentObject->uChildrenFound++;
                }
                else
                {
                    
                    PL_ASSERT(false); // shouldn't be possible
                }
                break;
            }

            case JSMN_ARRAY:
            {
                if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(cPtrJson, tPtrCurrentToken),
                        (uint32_t)tPtrCurrentToken->size
                    };
                    tNewJsonObject.psbcBuffer = &tPtrJsonOut->sbcBuffer;
                    strcpy(tNewJsonObject.acName, "UNNAMED ARRAY");
                    pl_sb_json_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrCurrentToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueOffsets, tPtrCurrentToken->size);
                    pl_sb_json_reserve(pl_sb_json_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrCurrentToken->size);
                    pl_sb_json_push(sbtObjectStack, &pl_sb_json_top(tPtrParentObject->sbtChildren));
                    tPtrParentObject->uChildrenFound++;
                }
                else if(tPtrParentObject->tType == PL_JSON_TYPE_STRING)
                {
                    // combining key/pair
                }
                else
                {
                    // shouldn't be possible
                    PL_ASSERT(false);
                }
                break;
            }
            
            default:
                break;
            }

            uCurrentTokenIndex++;
        }
    }

    pl_sb_json_free(sbtTokens);
    return true;
}

void
pl_unload_json(plJsonObject* tPtrJson)
{
    for(uint32_t i = 0; i < pl_sb_json_size(tPtrJson->sbtChildren); i++)
        pl_unload_json(&tPtrJson->sbtChildren[i]);

    if(tPtrJson->tType == PL_JSON_TYPE_ARRAY)
    {
        pl_sb_json_free(tPtrJson->sbuValueOffsets);
        pl_sb_json_free(tPtrJson->sbtChildren);
        pl_sb_json_free(tPtrJson->sbuValueLength);
    }
    else
    {
        tPtrJson->uValueOffset = 0;
        tPtrJson->uValueLength = 0;
    }

    tPtrJson->uChildCount = 0;
    tPtrJson->uChildrenFound = 0;

    pl_sb_json_free(tPtrJson->sbcBuffer);

    memset(tPtrJson->acName, 0, PL_JSON_MAX_NAME_LENGTH);
    tPtrJson->tType = PL_JSON_TYPE_UNSPECIFIED;
}

char*
pl_write_json(plJsonObject* tPtrJson, char* pcBuffer, uint32_t* puBufferSize)
{
    uint32_t uCursorPosition = 0;
    uint32_t uDepth = 0;
    if(pcBuffer)
        pl__write_json_object(tPtrJson, pcBuffer, puBufferSize, &uCursorPosition, &uDepth);
    else
        pl__check_json_object(tPtrJson, puBufferSize, &uCursorPosition, &uDepth);
    *puBufferSize = uCursorPosition;
    return pcBuffer;
}

plJsonObject*
pl_json_member_by_name(plJsonObject* tPtrJson, const char* pcName)
{

    for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
    {
        if(strncmp(pcName, tPtrJson->sbtChildren[i].acName, strlen(tPtrJson->sbtChildren[i].acName)) == 0)
            return &tPtrJson->sbtChildren[i];
    }

    return NULL;
}

plJsonObject*
pl_json_member_by_index(plJsonObject* tPtrJson, uint32_t uIndex)
{
    PL_ASSERT(uIndex < tPtrJson->uChildCount);
    return &tPtrJson->sbtChildren[uIndex];
}

void
pl_json_member_list(plJsonObject* tPtrJson, char** cPtrListOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength)
{
    if(cPtrListOut)
    {
        for(uint32_t i = 0; i < pl_sb_json_size(tPtrJson->sbtChildren); i++)
            strcpy(cPtrListOut[i], tPtrJson->sbtChildren[i].acName);
    }

    if(uPtrSizeOut)
        *uPtrSizeOut = pl_sb_json_size(tPtrJson->sbtChildren);

    if(uPtrLength)
    {
        for(uint32_t i = 0; i < pl_sb_json_size(tPtrJson->sbtChildren); i++)
        {
            const uint32_t uLength = (uint32_t)strlen(tPtrJson->sbtChildren[i].acName);
            if(uLength > *uPtrLength) *uPtrLength = uLength;
        }  
    }
}

bool
pl_json_member_exist(plJsonObject* tPtrJson, const char* pcName)
{
    return pl_json_member_by_name(tPtrJson, pcName) != NULL;
}

int
pl_json_int_member(plJsonObject* tPtrJson, const char* pcName, int iDefaultValue)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        return pl_json_as_int(tPtrMember);

    return iDefaultValue;
}

uint32_t
pl_json_uint_member(plJsonObject* tPtrJson, const char* pcName, uint32_t uDefaultValue)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        return pl_json_as_uint(tPtrMember);
    return uDefaultValue;
}

float
pl_json_float_member(plJsonObject* tPtrJson, const char* pcName, float fDefaultValue)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        return pl_json_as_float(tPtrMember);
    return fDefaultValue;
}

double
pl_json_double_member(plJsonObject* tPtrJson, const char* pcName, double dDefaultValue)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        return pl_json_as_double(tPtrMember);
    return dDefaultValue;
}

char*
pl_json_string_member(plJsonObject* tPtrJson, const char* pcName, char* cPtrDefaultValue, uint32_t uLength)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
    {
        PL_ASSERT(uLength >= tPtrMember->uValueLength);
        memset(cPtrDefaultValue, 0, uLength);
        strncpy(cPtrDefaultValue, &(*tPtrMember->psbcBuffer)[tPtrMember->uValueOffset], tPtrMember->uValueLength);
    }
    return cPtrDefaultValue;
}

bool
pl_json_bool_member(plJsonObject* tPtrJson, const char* pcName, bool bDefaultValue)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        return pl_json_as_bool(tPtrMember);
    return bDefaultValue;
}

plJsonObject*
pl_json_member(plJsonObject* tPtrJson, const char* pcName)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
    {
        PL_ASSERT(tPtrMember->tType == PL_JSON_TYPE_OBJECT);
        return tPtrMember;
    }
    return NULL;    
}

plJsonObject*
pl_json_array_member(plJsonObject* tPtrJson, const char* pcName, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);
    if(uPtrSizeOut)
        *uPtrSizeOut = 0;

    if(tPtrMember)
    {
        PL_ASSERT(tPtrMember->tType == PL_JSON_TYPE_ARRAY);
        if(uPtrSizeOut)
            *uPtrSizeOut = tPtrMember->uChildCount;
        return tPtrMember->sbtChildren;
    }
    return NULL;
}

void
pl_json_int_array_member(plJsonObject* tPtrJson, const char* pcName, int* iPtrOut, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_int_array(tPtrMember, iPtrOut, uPtrSizeOut);
}

void
pl_json_uint_array_member(plJsonObject* tPtrJson, const char* pcName, uint32_t* uPtrOut, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_uint_array(tPtrMember, uPtrOut, uPtrSizeOut);
}

void
pl_json_float_array_member(plJsonObject* tPtrJson, const char* pcName, float* fPtrOut, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_float_array(tPtrMember, fPtrOut, uPtrSizeOut);
}

void
pl_json_double_array_member(plJsonObject* tPtrJson, const char* pcName, double* dPtrOut, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_double_array(tPtrMember, dPtrOut, uPtrSizeOut);
}

void
pl_json_string_array_member(plJsonObject* tPtrJson, const char* pcName, char** cPtrOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_string_array(tPtrMember, cPtrOut, uPtrSizeOut, uPtrLength);
}

void
pl_json_bool_array_member(plJsonObject* tPtrJson, const char* pcName, bool* bPtrOut, uint32_t* uPtrSizeOut)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
        pl_json_as_bool_array(tPtrMember, bPtrOut, uPtrSizeOut);
}

int
pl_json_as_int(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return (int)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset], NULL);
}

uint32_t
pl_json_as_uint(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return (uint32_t)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset], NULL);
}

float
pl_json_as_float(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return (float)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset], NULL);
}

double
pl_json_as_double(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset], NULL);
}

const char*
pl_json_as_string(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_STRING);
    return &(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset];
}

bool
pl_json_as_bool(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_BOOL);
    return (&(*tPtrJson->psbcBuffer)[tPtrJson->uValueOffset])[0] == 't';
}

void
pl_json_as_int_array(plJsonObject* tPtrJson, int* iPtrOut, uint32_t* uPtrSizeOut)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(uPtrSizeOut)
        *uPtrSizeOut = tPtrJson->uChildCount;

    if(iPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
            iPtrOut[i] = (int)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_uint_array(plJsonObject* tPtrJson, uint32_t* uPtrOut, uint32_t* uPtrSizeOut)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(uPtrSizeOut)
        *uPtrSizeOut = tPtrJson->uChildCount;

    if(uPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
            uPtrOut[i] = (uint32_t)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_float_array(plJsonObject* tPtrJson, float* fPtrOut, uint32_t* uPtrSizeOut)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(uPtrSizeOut)
    {
        *uPtrSizeOut = tPtrJson->uChildCount;
    }

    if(fPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
            fPtrOut[i] = (float)strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_double_array(plJsonObject* tPtrJson, double* dPtrOut, uint32_t* uPtrSizeOut)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(uPtrSizeOut)
        *uPtrSizeOut = tPtrJson->uChildCount;

    if(dPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
            dPtrOut[i] = strtod(&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_string_array(plJsonObject* tPtrJson, char** cPtrOut, uint32_t* uPtrSizeOut, uint32_t* uPtrLength)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(cPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
        {
            PL_ASSERT(*uPtrLength >= tPtrJson->sbuValueLength[i]);
            memset(cPtrOut[i], 0, *uPtrLength);
            strncpy(cPtrOut[i],&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]], tPtrJson->sbuValueLength[i]);
        }
    }
    else if(uPtrSizeOut)
        *uPtrSizeOut = tPtrJson->uChildCount;

    if(uPtrLength)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
        {
            if(tPtrJson->sbuValueLength[i] > *uPtrLength)
                *uPtrLength = tPtrJson->sbuValueLength[i];
        }       
    }
}

void
pl_json_as_bool_array(plJsonObject* tPtrJson, bool* bPtrOut, uint32_t* uPtrSizeOut)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_ARRAY);

    if(uPtrSizeOut)
        *uPtrSizeOut = tPtrJson->uChildCount;

    if(bPtrOut)
    {
        for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
            bPtrOut[i] = (&(*tPtrJson->psbcBuffer)[tPtrJson->sbuValueOffsets[i]])[0] == 't';
    }
}

void
pl_json_add_int_member(plJsonObject* tPtrJson, const char* pcName, int iValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;
        
    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%i", iValue);
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength);
    snprintf(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%i", iValue);

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint_member(plJsonObject* tPtrJson, const char* pcName, uint32_t uValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%u", uValue);
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength);
    snprintf(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength, "%u", uValue);

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_float_member(plJsonObject* tPtrJson, const char* pcName, float fValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%0.7f", fValue);
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength);
    snprintf(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength, "%0.7f", fValue);

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_double_member(plJsonObject* tPtrJson, const char* pcName, double dValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%0.15f", dValue);
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength);
    snprintf(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength, "%0.15f", dValue);

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_bool_member(plJsonObject* tPtrJson, const char* pcName, bool bValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_BOOL;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%s", bValue ? "true" : "false");
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength);
    snprintf(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength, bValue ? "true" : "false");

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_string_member(plJsonObject* tPtrJson, const char* pcName, const char* pcValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_STRING;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
    tNewJsonObject.uValueLength = (uint32_t)strlen(pcValue);
    pl_sb_json_resize(*tPtrJson->psbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    strncpy(&(*tPtrJson->psbcBuffer)[tNewJsonObject.uValueOffset], pcValue, tNewJsonObject.uValueLength);
    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_member(plJsonObject* tPtrJson, const char* pcName, plJsonObject* ptValue)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    snprintf(ptValue->acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    pl_sb_json_push(tPtrJson->sbtChildren, *ptValue); 
}

void
pl_json_add_member_array(plJsonObject* tPtrJson, const char* pcName, plJsonObject* ptValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    tNewJsonObject.sbtChildren = NULL;
    pl_sb_json_resize(tNewJsonObject.sbtChildren, uSize);

    for(uint32_t i = 0; i < uSize; i++)
        tNewJsonObject.sbtChildren[i] = ptValues[i];

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_int_array(plJsonObject* tPtrJson, const char* pcName, int* piValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%i", piValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "%i", piValues[i]);

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint_array(plJsonObject* tPtrJson, const char* pcName, uint32_t* puValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%u", puValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "%u", puValues[i]);

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_float_array(plJsonObject* tPtrJson, const char* pcName, float* pfValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%0.7f", pfValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "%0.7f", pfValues[i]);

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_double_array(plJsonObject* tPtrJson, const char* pcName, double* pdValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%0.15f", pdValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "%0.15f", pdValues[i]);

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_bool_array(plJsonObject* tPtrJson, const char* pcName, bool* pbValues, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%s", pbValues[i] ? "true" : "false");
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "%s", pbValues[i] ? "true" : "false");

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_string_array(plJsonObject* tPtrJson, const char* pcName, char** ppcBuffer, uint32_t uSize)
{
    tPtrJson->uChildCount++;
    tPtrJson->uChildrenFound++;
    tPtrJson->tType = PL_JSON_TYPE_OBJECT;

    if(tPtrJson->psbcBuffer == NULL)
        tPtrJson->psbcBuffer = &tPtrJson->sbcBuffer;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.psbcBuffer = tPtrJson->psbcBuffer;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_json_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_json_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_json_size(*tPtrJson->psbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "\"%s\"", ppcBuffer[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_json_resize(*tPtrJson->psbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&(*tPtrJson->psbcBuffer)[uValueOffset], uValueLength + 1, "\"%s\"", ppcBuffer[i]);

    }

    pl_sb_json_push(tPtrJson->sbtChildren, tNewJsonObject);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plJsonType
pl__get_json_token_object_type(const char* cPtrJson, jsmntok_t* tPtrToken)
{
    switch (tPtrToken->type)
    {
    case JSMN_ARRAY:  return PL_JSON_TYPE_ARRAY;
    case JSMN_OBJECT: return PL_JSON_TYPE_OBJECT;
    case JSMN_STRING: return PL_JSON_TYPE_STRING;
    case JSMN_PRIMITIVE:
        if     (cPtrJson[tPtrToken->start] == 'n')                                      { return PL_JSON_TYPE_NULL;}
        else if(cPtrJson[tPtrToken->start] == 't' || cPtrJson[tPtrToken->start] == 'f') { return PL_JSON_TYPE_BOOL;}
        else                                                                            { return PL_JSON_TYPE_NUMBER;}
    default:
        PL_ASSERT(false);
        break;
    }
    return PL_JSON_TYPE_UNSPECIFIED;
}

static void
pl__write_json_object(plJsonObject* ptJson, char* pcBuffer, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth)
{
    uint32_t uBufferSize = *puBufferSize;
    uint32_t uCursorPosition = *puCursor;

    switch(ptJson->tType)
    {

        case PL_JSON_TYPE_NULL:
        {
            int iSizeNeeded = snprintf(NULL, 0, "null");
            snprintf(&pcBuffer[uCursorPosition], iSizeNeeded + 1, "null");
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_BOOL:
        {
            int iSizeNeeded = snprintf(NULL, 0, "%s", (&(*ptJson->psbcBuffer)[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
            snprintf(&pcBuffer[uCursorPosition], iSizeNeeded + 1, "%s", (&(*ptJson->psbcBuffer)[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_NUMBER:
        {
            memcpy(&pcBuffer[uCursorPosition], &(*ptJson->psbcBuffer)[ptJson->uValueOffset], ptJson->uValueLength);
            uCursorPosition += ptJson->uValueLength;
            break;
        }

        case PL_JSON_TYPE_STRING:
        {
            int iSizeNeeded = snprintf(&pcBuffer[uCursorPosition], (int)ptJson->uValueLength + 2 + 1, "\"%s\"", &(*ptJson->psbcBuffer)[ptJson->uValueOffset]);
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_OBJECT:
        {

            *puDepth += 1;
            int iSizeNeeded = 1 + *puDepth * 4;
            pcBuffer[uCursorPosition] = '{';
            memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded - 1);
            uCursorPosition += iSizeNeeded;

            for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            {

                int iSizeNeeded3 = *puDepth * 4 + 1;
                pcBuffer[uCursorPosition] = '\n';
                memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded3);
                uCursorPosition += iSizeNeeded3;

                int iSizeNeeded2 = snprintf(NULL, 0, "\"%s\": ", ptJson->sbtChildren[i].acName);
                snprintf(&pcBuffer[uCursorPosition], iSizeNeeded2 + 1 , "\"%s\": ", ptJson->sbtChildren[i].acName);

                uCursorPosition += iSizeNeeded2;

                pl__write_json_object(&ptJson->sbtChildren[i], pcBuffer, &uBufferSize, &uCursorPosition, puDepth);

                if(i != ptJson->uChildCount - 1)
                {
                    pcBuffer[uCursorPosition] = ',';
                    pcBuffer[uCursorPosition + 1] = ' ';
                    uCursorPosition += 2;
                }
            }

            *puDepth -= 1;

            int iSizeNeeded3 = *puDepth * 4 + 2;
            pcBuffer[uCursorPosition] = '\n';
            memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded3 - 2);
            uCursorPosition += iSizeNeeded3;
            pcBuffer[uCursorPosition - 1] = '}';
            break;
        }

        case PL_JSON_TYPE_ARRAY:
        {

            *puDepth += 1;
            int iSizeNeeded = 2 + *puDepth * 4;
            pcBuffer[uCursorPosition] = '[';
            pcBuffer[uCursorPosition + 1] = '\n';
            memset(&pcBuffer[uCursorPosition + 2], 0x20, iSizeNeeded - 2);
            uCursorPosition += iSizeNeeded;


            if(ptJson->sbuValueOffsets)
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {

                    // pl__write_json_object(&ptJson->sbtChildren[i], pcBuffer, &uBufferSize, &uCursorPosition, puDepth);

                    const char* cPtrPrevChar = &(*ptJson->psbcBuffer)[ptJson->sbuValueOffsets[i]];
                    char cPreviousChar = ' ';
                    if(cPtrPrevChar)
                    {
                        const char* cPtrPrevCharAddr = cPtrPrevChar - 1;
                        cPreviousChar = cPtrPrevCharAddr[0];
                    }


                    if(cPreviousChar == '\"')
                    {
                        int iSizeNeeded2 = ptJson->sbuValueLength[i] + 2;
                        snprintf(&pcBuffer[uCursorPosition], iSizeNeeded2 + 1, "\"%s\"", &(*ptJson->psbcBuffer)[ptJson->sbuValueOffsets[i]]);
                        uCursorPosition += iSizeNeeded2;
                    }
                    else
                    {
                        memcpy(&pcBuffer[uCursorPosition], &(*ptJson->psbcBuffer)[ptJson->sbuValueOffsets[i]], ptJson->sbuValueLength[i]);
                        uCursorPosition += ptJson->sbuValueLength[i];
                    }
                    
                    if(i != ptJson->uChildCount - 1)
                    {
                        pcBuffer[uCursorPosition] = ',';
                        pcBuffer[uCursorPosition + 1] = ' ';
                        uCursorPosition += 2;
                    }
                }
            }
            else
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {

                    pl__write_json_object(&ptJson->sbtChildren[i], pcBuffer, &uBufferSize, &uCursorPosition, puDepth);

                    if(i != ptJson->uChildCount - 1)
                    {
                        pcBuffer[uCursorPosition] = ',';
                        pcBuffer[uCursorPosition + 1] = ' ';
                        uCursorPosition += 2;
                    }
                }
            }
 
            *puDepth -= 1;

            int iSizeNeeded3 = *puDepth * 4 + 2;
            pcBuffer[uCursorPosition] = '\n';
            memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded3 - 2);
            uCursorPosition += iSizeNeeded3;
            pcBuffer[uCursorPosition - 1] = ']';
            break;
        }
    }

    *puBufferSize = uBufferSize;
    *puCursor = uCursorPosition;
}

static void
pl__check_json_object(plJsonObject* ptJson, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth)
{
    uint32_t uBufferSize = *puBufferSize;
    uint32_t uCursorPosition = *puCursor;

    switch(ptJson->tType)
    {

        case PL_JSON_TYPE_NULL:
        {
            int iSizeNeeded = snprintf(NULL, 0, "null");
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_BOOL:
        {
            
            int iSizeNeeded = snprintf(NULL, 0, "%s", (&(*ptJson->psbcBuffer)[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_NUMBER:
        {
            uCursorPosition += ptJson->uValueLength;
            break;
        }

        case PL_JSON_TYPE_STRING:
        {
            int iSizeNeeded = (int)ptJson->uValueLength + 2;
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_OBJECT:
        {

            *puDepth += 1;
            int iSizeNeeded = 1 + *puDepth * 4;
            uCursorPosition += iSizeNeeded;

            for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            {

                int iSizeNeeded3 = *puDepth * 4 + 1;
                uCursorPosition += iSizeNeeded3;

                int iSizeNeeded2 = snprintf(NULL, 0, "\"%s\": ", ptJson->sbtChildren[i].acName);
                uCursorPosition += iSizeNeeded2;

                pl__check_json_object(&ptJson->sbtChildren[i], &uBufferSize, &uCursorPosition, puDepth);

                if(i != ptJson->uChildCount - 1)
                    uCursorPosition += 2;
            }

            *puDepth -= 1;

            int iSizeNeeded3 = *puDepth * 4 + 2;
            uCursorPosition += iSizeNeeded3;
            break;
        }

        case PL_JSON_TYPE_ARRAY:
        {

            *puDepth += 1;
            int iSizeNeeded = 2 + *puDepth * 4;
            uCursorPosition += iSizeNeeded;

            if(ptJson->sbuValueOffsets)
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {

                    // pl__check_json_object(&ptJson->sbtChildren[i], &uBufferSize, &uCursorPosition, puDepth);

                    const char* cPtrPrevChar = &(*ptJson->psbcBuffer)[ptJson->sbuValueOffsets[i]];
                    char cPreviousChar = ' ';
                    if(cPtrPrevChar)
                    {
                        const char* cPtrPrevCharAddr = cPtrPrevChar - 1;
                        cPreviousChar = cPtrPrevCharAddr[0];
                    }

                    if(cPreviousChar == '\"')
                    {
                        int iSizeNeeded2 = ptJson->sbuValueLength[i] + 2;
                        uCursorPosition += iSizeNeeded2;
                    }
                    else
                        uCursorPosition += ptJson->sbuValueLength[i];
                    

                    if(i != ptJson->uChildCount - 1)
                        uCursorPosition += 2;
                }
            }
            else
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {

                    pl__check_json_object(&ptJson->sbtChildren[i], &uBufferSize, &uCursorPosition, puDepth);

                    if(i != ptJson->uChildCount - 1)
                        uCursorPosition += 2;
                }
            }
 
            *puDepth -= 1;
            int iSizeNeeded3 = *puDepth * 4 + 2;
            uCursorPosition += iSizeNeeded3;
            break;
        }
    }

    *puBufferSize = uBufferSize;
    *puCursor = uCursorPosition;
}

#endif // PL_JSON_IMPLEMENTATION
