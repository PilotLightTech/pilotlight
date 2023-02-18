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
#define PL_JSON_VERSION    "0.1.0"
#define PL_JSON_VERSION_NUM 00100

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
plJsonObject* pl_json_array_member       (plJsonObject* tPtrJson, const char* pcName);

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

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plJsonObject
{
    char          acName[PL_JSON_MAX_NAME_LENGTH];
    plJsonType    tType;
    plJsonObject* sbtChildren;
    uint32_t      uChildrenFound;
    uint32_t      uChildCount;
    union
    {
        struct
        {
            const char** sbcPtrValue;
            uint32_t*    sbuValueLength;
        };

        struct
        {
            const char* cPtrValue;
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
#include "pl_ds.h"

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plJsonType pl__get_json_token_object_type(const char* cPtrJson, jsmntok_t* tPtrToken);

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
    pl_sb_resize(sbtTokens, 512);

    jsmn_init(&tP);

    int iResult = 0;
    while(true)
    {
        iResult = jsmn_parse(&tP, cPtrJson, strlen(cPtrJson), sbtTokens, pl_sb_size(sbtTokens));

        if(iResult == JSMN_ERROR_INVAL)
        {
            PL_ASSERT(false);
        }
        else if(iResult == JSMN_ERROR_NOMEM)
        {
            pl_sb_add_n(sbtTokens, 256);
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
    tPtrJsonOut->uChildCount = sbtTokens[uCurrentTokenIndex].size;
    strcpy(tPtrJsonOut->acName, "ROOT");
    pl_sb_reserve(tPtrJsonOut->sbtChildren, sbtTokens[uCurrentTokenIndex].size);
    pl_sb_push(sbtObjectStack, tPtrJsonOut);
    while(uCurrentTokenIndex < (uint32_t)iResult - 1)
    {

        if(pl_sb_top(sbtObjectStack)->uChildrenFound == pl_sb_top(sbtObjectStack)->uChildCount)
            pl_sb_pop(sbtObjectStack);
        else
        {
            
            plJsonObject* tPtrParentObject = pl_sb_top(sbtObjectStack);

            jsmntok_t* tPtrCurrentToken = &sbtTokens[uCurrentTokenIndex];
            jsmntok_t* tPtrNextToken = &sbtTokens[uCurrentTokenIndex + 1];

            switch (tPtrCurrentToken->type)
            {

            // value
            case JSMN_PRIMITIVE:
                if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    pl_sb_push(tPtrParentObject->sbcPtrValue, &cPtrJson[tPtrCurrentToken->start]);
                    pl_sb_push(tPtrParentObject->sbuValueLength, tPtrCurrentToken->end - tPtrCurrentToken->start);
                    tPtrParentObject->uChildrenFound++;
                }
                else
                {
                    tPtrParentObject->cPtrValue = &cPtrJson[tPtrCurrentToken->start];
                    tPtrParentObject->uValueLength = tPtrCurrentToken->end - tPtrCurrentToken->start;
                    tPtrParentObject->uChildrenFound++;
                    pl_sb_pop(sbtObjectStack);
                }
                break;

            case JSMN_STRING:
            {
                // value
                if(tPtrCurrentToken->size == 0)
                {
                    if(tPtrParentObject->tType == PL_JSON_TYPE_ARRAY)
                    {
                        tPtrParentObject->uChildrenFound++;
                        pl_sb_push(tPtrParentObject->sbcPtrValue, &cPtrJson[tPtrCurrentToken->start]);
                        pl_sb_push(tPtrParentObject->sbuValueLength, tPtrCurrentToken->end - tPtrCurrentToken->start);
                    }
                    else
                    {
                        tPtrParentObject->uChildrenFound++;
                        tPtrParentObject->cPtrValue = &cPtrJson[tPtrCurrentToken->start];
                        tPtrParentObject->uValueLength = tPtrCurrentToken->end - tPtrCurrentToken->start;
                        pl_sb_pop(sbtObjectStack);
                    }
                }

                // key
                else
                {
                    plJsonObject tNewJsonObject = {
                        .uChildCount = tPtrNextToken->size,
                        .tType       = pl__get_json_token_object_type(cPtrJson, tPtrNextToken)
                    };
                    if(tNewJsonObject.uChildCount == 0)
                    {
                        tNewJsonObject.uChildrenFound--;
                    }
                    tPtrParentObject->uChildrenFound++;
                    strncpy(tNewJsonObject.acName, &cPtrJson[tPtrCurrentToken->start], tPtrCurrentToken->end - tPtrCurrentToken->start);
                    pl_sb_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrNextToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbcPtrValue, tPtrNextToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrNextToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(tPtrParentObject->sbtChildren));

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
                        .uChildCount = tPtrCurrentToken->size,
                        .tType       = pl__get_json_token_object_type(cPtrJson, tPtrCurrentToken)
                    };
                    strcpy(tNewJsonObject.acName, "UNNAMED OBJECT");
                    pl_sb_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbcPtrValue, tPtrCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrCurrentToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(tPtrParentObject->sbtChildren));
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
                        .uChildCount = tPtrCurrentToken->size,
                        .tType       = pl__get_json_token_object_type(cPtrJson, tPtrCurrentToken)
                    };
                    strcpy(tNewJsonObject.acName, "UNNAMED ARRAY");
                    pl_sb_push(tPtrParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbtChildren, tPtrCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbcPtrValue, tPtrCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(tPtrParentObject->sbtChildren).sbuValueLength, tPtrCurrentToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(tPtrParentObject->sbtChildren));
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

    pl_sb_free(sbtTokens);
    return true;
}

void
pl_unload_json(plJsonObject* tPtrJson)
{
    for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
        pl_unload_json(&tPtrJson->sbtChildren[i]);

    if(tPtrJson->tType == PL_JSON_TYPE_ARRAY)
    {
        pl_sb_free(tPtrJson->sbcPtrValue);
        pl_sb_free(tPtrJson->sbtChildren);
        pl_sb_free(tPtrJson->sbuValueLength);
    }
    else
    {
        tPtrJson->cPtrValue = NULL;
        tPtrJson->uValueLength = 0;
    }

    tPtrJson->uChildCount = 0;
    tPtrJson->uChildrenFound = 0;

    memset(tPtrJson->acName, 0, PL_JSON_MAX_NAME_LENGTH);
    tPtrJson->tType = PL_JSON_TYPE_UNSPECIFIED;
}

plJsonObject*
pl_json_member_by_name(plJsonObject* tPtrJson, const char* pcName)
{

    for(uint32_t i = 0; i < tPtrJson->uChildCount; i++)
    {
        if(strncmp(pcName, tPtrJson->sbtChildren[i].acName, strlen(pcName)) == 0)
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
        for(uint32_t i = 0; i < pl_sb_size(tPtrJson->sbtChildren); i++)
            strcpy(cPtrListOut[i], tPtrJson->sbtChildren[i].acName);
    }

    if(uPtrSizeOut)
        *uPtrSizeOut = pl_sb_size(tPtrJson->sbtChildren);

    if(uPtrLength)
    {
        for(uint32_t i = 0; i < pl_sb_size(tPtrJson->sbtChildren); i++)
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
        strncpy(cPtrDefaultValue, tPtrMember->cPtrValue, tPtrMember->uValueLength);
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
pl_json_array_member(plJsonObject* tPtrJson, const char* pcName)
{
    plJsonObject* tPtrMember = pl_json_member_by_name(tPtrJson, pcName);

    if(tPtrMember)
    {
        PL_ASSERT(tPtrMember->tType == PL_JSON_TYPE_ARRAY);
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
    return (int)strtod(tPtrJson->cPtrValue, NULL);
}

uint32_t
pl_json_as_uint(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return (uint32_t)strtod(tPtrJson->cPtrValue, NULL);
}

float
pl_json_as_float(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return (float)strtod(tPtrJson->cPtrValue, NULL);
}

double
pl_json_as_double(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_NUMBER);
    return strtod(tPtrJson->cPtrValue, NULL);
}

const char*
pl_json_as_string(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_STRING);
    return tPtrJson->cPtrValue;
}

bool
pl_json_as_bool(plJsonObject* tPtrJson)
{
    PL_ASSERT(tPtrJson->tType == PL_JSON_TYPE_BOOL);
    return tPtrJson->cPtrValue[0] == 't';
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
            iPtrOut[i] = (int)strtod(tPtrJson->sbcPtrValue[i], NULL);
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
            uPtrOut[i] = (uint32_t)strtod(tPtrJson->sbcPtrValue[i], NULL);
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
            fPtrOut[i] = (float)strtod(tPtrJson->sbcPtrValue[i], NULL);
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
            dPtrOut[i] = strtod(tPtrJson->sbcPtrValue[i], NULL);
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
            strncpy(cPtrOut[i], tPtrJson->sbcPtrValue[i], tPtrJson->sbuValueLength[i]);
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
            bPtrOut[i] = tPtrJson->sbcPtrValue[i][0] == 't';
    }
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

#endif // PL_JSON_IMPLEMENTATION