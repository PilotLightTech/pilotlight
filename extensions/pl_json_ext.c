/*
   pl_json_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h> // FLT_MAX
#include <inttypes.h>
#include "pl.h"
#include "pl_json_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    #ifndef PL_ALLOC
        #define PL_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

#ifndef CGLTF_IMPLEMENTATION

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
#endif /* JSMN_H */


//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal types
// [SECTION] stretchy buffer
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <float.h>  // FLT_MAX
#include <stdio.h>  // sprintf

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_JSON_MAX_NAME_LENGTH
    #define PL_JSON_MAX_NAME_LENGTH 256
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal types
//-----------------------------------------------------------------------------

typedef struct _plJsonObject
{
    plJsonType    tType;
    uint32_t      uChildCount;
    plJsonObject* ptRootObject;
    char          acName[PL_JSON_MAX_NAME_LENGTH]; 
    plJsonObject* sbtChildren;
    uint32_t      uChildrenFound;
    char*         sbcBuffer;
    uint32_t*     sbuValueOffsets;
    uint32_t*     sbuValueLength;
    uint32_t      uValueOffset;
    uint32_t      uValueLength;
    plJsonType    tArrayElementType;
} plJsonObject;

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plJsonType pl__get_json_token_object_type(const char* pcJson, jsmntok_t*);
static void       pl__write_json_object(plJsonObject* ptJson, char* pcBuffer, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth);
static void       pl__check_json_object(plJsonObject* ptJson, uint32_t* puBufferSize, uint32_t* puCursor, uint32_t* puDepth);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plJsonObject*
pl_json_new_root_object(const char* pcName)
{
    plJsonObject* ptJson = (plJsonObject*)PL_ALLOC(sizeof(plJsonObject));
    memset(ptJson, 0, sizeof(plJsonObject));
    ptJson->tType = PL_JSON_TYPE_OBJECT;
    ptJson->ptRootObject = ptJson;
    strncpy(ptJson->acName, pcName, 256);
    return ptJson;
}

bool
pl_json_load(const char* pcJson, plJsonObject** pptJsonOut)
{
    jsmn_parser tP = {0};
    jsmntok_t* sbtTokens = NULL;
    pl_sb_resize(sbtTokens, 512);

    jsmn_init(&tP);

    int iResult = 0;
    while(true)
    {
        iResult = jsmn_parse(&tP, pcJson, strlen(pcJson), sbtTokens, pl_sb_size(sbtTokens));

        if(iResult == JSMN_ERROR_INVAL)
        {
            pl_sb_free(sbtTokens);
            PL_ASSERT(false);
            return false;
            
        }
        else if(iResult == JSMN_ERROR_NOMEM)
        {
            pl_sb_add_n(sbtTokens, 256);
        }
        else if(iResult == JSMN_ERROR_PART)
        {
            pl_sb_free(sbtTokens);
            PL_ASSERT(false);
            return false;
        }
        else
        {
            break;
        }
    }

    uint32_t uLayer = 0;
    uint32_t uCurrentTokenIndex = 0;
    plJsonObject** sbtObjectStack = NULL;
    *pptJsonOut = (plJsonObject*)PL_ALLOC(sizeof(plJsonObject));
    memset(*pptJsonOut, 0, sizeof(plJsonObject));
    plJsonObject* ptJsonOut = *pptJsonOut;
    if(ptJsonOut == NULL)
    {
        pl_sb_free(sbtTokens);
        return false;
    }
    ptJsonOut->ptRootObject = ptJsonOut;
    pl_sb_reserve(ptJsonOut->sbcBuffer, strlen(pcJson));
    ptJsonOut->tType = pl__get_json_token_object_type(pcJson, &sbtTokens[uCurrentTokenIndex]);
    if(ptJsonOut->tType == PL_JSON_TYPE_ARRAY)
        ptJsonOut->uChildCount = 1;
    else
        ptJsonOut->uChildCount = sbtTokens[uCurrentTokenIndex].size;
    strcpy(ptJsonOut->acName, "ROOT");
    pl_sb_reserve(ptJsonOut->sbtChildren, ptJsonOut->uChildCount);
    pl_sb_push(sbtObjectStack, ptJsonOut);
    while(uCurrentTokenIndex < (uint32_t)iResult)
    {

        if(pl_sb_top(sbtObjectStack)->uChildrenFound == pl_sb_top(sbtObjectStack)->uChildCount)
            pl_sb_pop(sbtObjectStack);
        else
        {
            
            plJsonObject* ptParentObject = pl_sb_top(sbtObjectStack);

            jsmntok_t* ptCurrentToken = &sbtTokens[uCurrentTokenIndex];
            jsmntok_t* ptNextToken = &sbtTokens[uCurrentTokenIndex + 1];

            switch (ptCurrentToken->type)
            {

            // value
            case JSMN_PRIMITIVE:
                if(ptParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    const uint32_t uBufferLocation = pl_sb_size(ptJsonOut->sbcBuffer);
                    const uint32_t uValueLength = (uint32_t)(ptCurrentToken->end - ptCurrentToken->start);
                    pl_sb_resize(ptJsonOut->sbcBuffer, uBufferLocation + uValueLength + 1);
                    memcpy(&ptJsonOut->sbcBuffer[uBufferLocation], &pcJson[ptCurrentToken->start], uValueLength);
                    ptJsonOut->sbcBuffer[uBufferLocation + uValueLength] = '\0';
                    pl_sb_push(ptParentObject->sbuValueOffsets, uBufferLocation);
                    pl_sb_push(ptParentObject->sbuValueLength, uValueLength);
                    ptParentObject->uChildrenFound++;
                }
                else
                {
                    const uint32_t uBufferLocation = pl_sb_size(ptJsonOut->sbcBuffer);
                    const uint32_t uValueLength = (uint32_t)(ptCurrentToken->end - ptCurrentToken->start);
                    pl_sb_resize(ptJsonOut->sbcBuffer, (uint32_t)(uBufferLocation + uValueLength + 1));
                    memcpy(&ptJsonOut->sbcBuffer[uBufferLocation], &pcJson[ptCurrentToken->start], uValueLength);
                    ptJsonOut->sbcBuffer[uBufferLocation + uValueLength] = '\0';
                    ptParentObject->uValueOffset = uBufferLocation;
                    ptParentObject->uValueLength = uValueLength;
                    ptParentObject->uChildrenFound++;
                    pl_sb_pop(sbtObjectStack);
                }
                break;

            case JSMN_STRING:
            {
                // value
                if(ptCurrentToken->size == 0)
                {
                    if(ptParentObject->tType == PL_JSON_TYPE_ARRAY)
                    {
                        const uint32_t uBufferLocation = pl_sb_size(ptJsonOut->sbcBuffer);
                        const uint32_t uValueLength = (uint32_t)(ptCurrentToken->end - ptCurrentToken->start);
                        pl_sb_resize(ptJsonOut->sbcBuffer, (uint32_t)(uBufferLocation + uValueLength + 1));
                        memcpy(&ptJsonOut->sbcBuffer[uBufferLocation], &pcJson[ptCurrentToken->start], uValueLength);
                        ptJsonOut->sbcBuffer[uBufferLocation + uValueLength] = '\0';
                        pl_sb_push(ptParentObject->sbuValueOffsets, uBufferLocation);
                        pl_sb_push(ptParentObject->sbuValueLength, uValueLength);
                        ptParentObject->uChildrenFound++;
                    }
                    else
                    {
                        const uint32_t uBufferLocation = pl_sb_size(ptJsonOut->sbcBuffer);
                        const uint32_t uValueLength = (uint32_t)(ptCurrentToken->end - ptCurrentToken->start);
                        pl_sb_resize(ptJsonOut->sbcBuffer, (uint32_t)(uBufferLocation + uValueLength + 1));
                        memcpy(&ptJsonOut->sbcBuffer[uBufferLocation], &pcJson[ptCurrentToken->start], uValueLength);
                        ptJsonOut->sbcBuffer[uBufferLocation + uValueLength] = '\0';
                        ptParentObject->uValueOffset = uBufferLocation;
                        ptParentObject->uValueLength = uValueLength;
                        ptParentObject->uChildrenFound++;
                        pl_sb_pop(sbtObjectStack);
                    }
                }

                // key
                else
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(pcJson, ptNextToken),
                        (uint32_t)ptNextToken->size
                    };
                    tNewJsonObject.ptRootObject = ptJsonOut;
                    if(tNewJsonObject.uChildCount == 0)
                    {
                        tNewJsonObject.uChildrenFound--;
                    }
                    ptParentObject->uChildrenFound++;
                    strncpy(tNewJsonObject.acName, &pcJson[ptCurrentToken->start], ptCurrentToken->end - ptCurrentToken->start);
                    pl_sb_push(ptParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbtChildren, ptNextToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueOffsets, ptNextToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueLength, ptNextToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(ptParentObject->sbtChildren));

                    if(tNewJsonObject.tType == PL_JSON_TYPE_ARRAY)
                    {
                        uCurrentTokenIndex++;
                        if(tNewJsonObject.uChildCount == 0) // empty array fix
                        {
                            pl_sb_pop(sbtObjectStack);
                        }
                    }
                    else if (tNewJsonObject.tType == PL_JSON_TYPE_OBJECT)
                    {
                        uCurrentTokenIndex++;
                        if(tNewJsonObject.uChildCount == 0) // empty object fix
                        {
                            pl_sb_pop(sbtObjectStack);
                        }
                    }
                    
                }
                break;
            }

            case JSMN_OBJECT:
            {
                if(ptParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(pcJson, ptCurrentToken),
                        (uint32_t)ptCurrentToken->size
                    };
                    tNewJsonObject.ptRootObject = ptJsonOut;
                    strcpy(tNewJsonObject.acName, "UNNAMED OBJECT");
                    pl_sb_push(ptParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbtChildren, ptCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueOffsets, ptCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueLength, ptCurrentToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(ptParentObject->sbtChildren));
                    ptParentObject->uChildrenFound++;
                }
                else if(ptParentObject->tType == PL_JSON_TYPE_OBJECT)
                {
                    // combining key/pair
                    // ptParentObject->uChildrenFound++;
                }
                else
                {                
                    pl_sb_free(sbtTokens);
                    PL_ASSERT(false); // shouldn't be possible
                    return false;
                }
                break;
            }

            case JSMN_ARRAY:
            {
                if(ptParentObject->tType == PL_JSON_TYPE_ARRAY)
                {
                    plJsonObject tNewJsonObject = {
                        pl__get_json_token_object_type(pcJson, ptCurrentToken),
                        (uint32_t)ptCurrentToken->size
                    };
                    tNewJsonObject.ptRootObject = ptJsonOut;
                    strcpy(tNewJsonObject.acName, "UNNAMED ARRAY");
                    pl_sb_push(ptParentObject->sbtChildren, tNewJsonObject);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbtChildren, ptCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueOffsets, ptCurrentToken->size);
                    pl_sb_reserve(pl_sb_top(ptParentObject->sbtChildren).sbuValueLength, ptCurrentToken->size);
                    pl_sb_push(sbtObjectStack, &pl_sb_top(ptParentObject->sbtChildren));
                    ptParentObject->uChildrenFound++;
                }
                else if(ptParentObject->tType == PL_JSON_TYPE_STRING)
                {
                    // combining key/pair
                }
                else
                {
                    pl_sb_free(sbtTokens);
                    PL_ASSERT(false); // shouldn't be possible
                    return false;
                }
                break;
            }
            
            default:
                break;
            }

            uCurrentTokenIndex++;
        }
    }

    pl_sb_free(sbtObjectStack);
    pl_sb_free(sbtTokens);
    return true;
}

static void
pl__free_json(plJsonObject* ptJson)
{
    for(uint32_t i = 0; i < pl_sb_size(ptJson->sbtChildren); i++)
        pl__free_json(&ptJson->sbtChildren[i]);

    pl_sb_free(ptJson->sbuValueOffsets);
    pl_sb_free(ptJson->sbtChildren);
    pl_sb_free(ptJson->sbuValueLength);
    ptJson->uValueOffset = 0;
    ptJson->uValueLength = 0;
    ptJson->uChildCount = 0;
    ptJson->uChildrenFound = 0;

    pl_sb_free(ptJson->sbcBuffer);

    memset(ptJson->acName, 0, PL_JSON_MAX_NAME_LENGTH);
    ptJson->tType = PL_JSON_TYPE_UNSPECIFIED;
}

void
pl_json_unload(plJsonObject** pptJson)
{
    plJsonObject* ptJson = *pptJson;
    for(uint32_t i = 0; i < pl_sb_size(ptJson->sbtChildren); i++)
    {
        pl__free_json(&ptJson->sbtChildren[i]);
    }

    pl_sb_free(ptJson->sbuValueOffsets);
    pl_sb_free(ptJson->sbtChildren);
    pl_sb_free(ptJson->sbuValueLength);
    ptJson->uValueOffset = 0;
    ptJson->uValueLength = 0;
    ptJson->uChildCount = 0;
    ptJson->uChildrenFound = 0;

    pl_sb_free(ptJson->sbcBuffer);

    memset(ptJson->acName, 0, PL_JSON_MAX_NAME_LENGTH);
    ptJson->tType = PL_JSON_TYPE_UNSPECIFIED;
    PL_FREE(ptJson);
    *pptJson = NULL;
}

char*
pl_json_write(plJsonObject* ptJson, char* pcBuffer, uint32_t* puBufferSize)
{
    uint32_t uCursorPosition = 0;
    uint32_t uDepth = 0;
    if(pcBuffer)
        pl__write_json_object(ptJson, pcBuffer, puBufferSize, &uCursorPosition, &uDepth);
    else
        pl__check_json_object(ptJson, puBufferSize, &uCursorPosition, &uDepth);
    *puBufferSize = uCursorPosition;
    return pcBuffer;
}

plJsonObject*
pl_json_member_by_name(plJsonObject* ptJson, const char* pcName)
{

    for(uint32_t i = 0; i < ptJson->uChildCount; i++)
    {
        if(strncmp(pcName, ptJson->sbtChildren[i].acName, PL_JSON_MAX_NAME_LENGTH) == 0)
            return &ptJson->sbtChildren[i];
    }

    return NULL;
}

plJsonObject*
pl_json_member_by_index(plJsonObject* ptJson, uint32_t uIndex)
{
    if(uIndex < ptJson->uChildCount)
        return &ptJson->sbtChildren[uIndex];
    return NULL;
}

void
pl_json_member_list(plJsonObject* ptJson, char** ppcListOut, uint32_t* puSizeOut, uint32_t* puLength)
{
    if(ppcListOut)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptJson->sbtChildren); i++)
            strcpy(ppcListOut[i], ptJson->sbtChildren[i].acName);
    }

    if(puSizeOut)
        *puSizeOut = pl_sb_size(ptJson->sbtChildren);

    if(puLength)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptJson->sbtChildren); i++)
        {
            const uint32_t uLength = (uint32_t)strlen(ptJson->sbtChildren[i].acName);
            if(uLength > *puLength) *puLength = uLength;
        }  
    }
}

plJsonType
pl_json_get_type(plJsonObject* ptJson)
{
    return ptJson->tType;
}

const char*
pl_json_get_name(plJsonObject* ptJson)
{
    return ptJson->acName;
}

bool
pl_json_member_exist(plJsonObject* ptJson, const char* pcName)
{
    return pl_json_member_by_name(ptJson, pcName) != NULL;
}

int
pl_json_int_member(plJsonObject* ptJson, const char* pcName, int iDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_int(ptMember);

    return iDefaultValue;
}

uint32_t
pl_json_uint32_member(plJsonObject* ptJson, const char* pcName, uint32_t uDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_uint32(ptMember);
    return uDefaultValue;
}

uint64_t
pl_json_uint64_member(plJsonObject* ptJson, const char* pcName, uint64_t uDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_uint64(ptMember);
    return uDefaultValue;
}

float
pl_json_float_member(plJsonObject* ptJson, const char* pcName, float fDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_float(ptMember);
    return fDefaultValue;
}

double
pl_json_double_member(plJsonObject* ptJson, const char* pcName, double dDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_double(ptMember);
    return dDefaultValue;
}

char*
pl_json_string_member(plJsonObject* ptJson, const char* pcName, char* pcDefaultValue, uint32_t uLength)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
    {
        if(uLength < ptMember->uValueLength)
            return NULL;
        memset(pcDefaultValue, 0, uLength);
        strncpy(pcDefaultValue, &ptMember->ptRootObject->sbcBuffer[ptMember->uValueOffset], ptMember->uValueLength);
    }
    return pcDefaultValue;
}

bool
pl_json_bool_member(plJsonObject* ptJson, const char* pcName, bool bDefaultValue)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        return pl_json_as_bool(ptMember);
    return bDefaultValue;
}

plJsonObject*
pl_json_member(plJsonObject* ptJson, const char* pcName)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);
    return ptMember;
}

plJsonObject*
pl_json_array_member(plJsonObject* ptJson, const char* pcName, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);
    if(puSizeOut)
        *puSizeOut = 0;

    if(ptMember)
    {
        PL_ASSERT(ptMember->tType == PL_JSON_TYPE_ARRAY);
        if(ptMember->tType == PL_JSON_TYPE_ARRAY)
        {
            if(puSizeOut)
                *puSizeOut = ptMember->uChildCount;
            return ptMember;
        }
    }
    return NULL;
}

void
pl_json_int_array_member(plJsonObject* ptJson, const char* pcName, int* piOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_int_array(ptMember, piOut, puSizeOut);
}

void
pl_json_uint32_array_member(plJsonObject* ptJson, const char* pcName, uint32_t* puOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_uint32_array(ptMember, puOut, puSizeOut);
}

void
pl_json_uint64_array_member(plJsonObject* ptJson, const char* pcName, uint64_t* puOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_uint64_array(ptMember, puOut, puSizeOut);
}

void
pl_json_float_array_member(plJsonObject* ptJson, const char* pcName, float* pfOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_float_array(ptMember, pfOut, puSizeOut);
}

void
pl_json_double_array_member(plJsonObject* ptJson, const char* pcName, double* pdOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_double_array(ptMember, pdOut, puSizeOut);
}

void
pl_json_string_array_member(plJsonObject* ptJson, const char* pcName, char** pcOut, uint32_t* puSizeOut, uint32_t* puLength)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_string_array(ptMember, pcOut, puSizeOut, puLength);
}

void
pl_json_bool_array_member(plJsonObject* ptJson, const char* pcName, bool* pbOut, uint32_t* puSizeOut)
{
    plJsonObject* ptMember = pl_json_member_by_name(ptJson, pcName);

    if(ptMember)
        pl_json_as_bool_array(ptMember, pbOut, puSizeOut);
}

int
pl_json_as_int(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_NUMBER);
    if(ptJson->tType == PL_JSON_TYPE_NUMBER)
        return (int)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], NULL);
    return 0;
}

uint32_t
pl_json_as_uint32(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_NUMBER);
    if(ptJson->tType == PL_JSON_TYPE_NUMBER)
        return (uint32_t)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], NULL);
    return UINT32_MAX;
}

uint64_t
pl_json_as_uint64(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_NUMBER);
    if(ptJson->tType == PL_JSON_TYPE_NUMBER)
        return (uint64_t)strtoull(&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], NULL, 0);
    return UINT64_MAX;
}

float
pl_json_as_float(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_NUMBER);
    if(ptJson->tType == PL_JSON_TYPE_NUMBER)
        return (float)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], NULL);
    return FLT_MAX;
}

double
pl_json_as_double(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_NUMBER);
    if(ptJson->tType == PL_JSON_TYPE_NUMBER)
        return strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], NULL);
    return DBL_MAX;
}

const char*
pl_json_as_string(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_STRING);
    if(ptJson->tType == PL_JSON_TYPE_STRING)
        return &ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset];
    return NULL;
}

bool
pl_json_as_bool(plJsonObject* ptJson)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_BOOL);
    if(ptJson->tType == PL_JSON_TYPE_BOOL)
        return (&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset])[0] == 't';
    return false;
}

void
pl_json_as_int_array(plJsonObject* ptJson, int* piOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(piOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            piOut[i] = (int)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_uint32_array(plJsonObject* ptJson, uint32_t* puOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(puOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            puOut[i] = (uint32_t)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_uint64_array(plJsonObject* ptJson, uint64_t* puOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(puOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            puOut[i] = (uint64_t)strtoull(&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], NULL, 0);
    }
}

void
pl_json_as_float_array(plJsonObject* ptJson, float* pfOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(pfOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            pfOut[i] = (float)strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_double_array(plJsonObject* ptJson, double* pdOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(pdOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            pdOut[i] = strtod(&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], NULL);
    }
}

void
pl_json_as_string_array(plJsonObject* ptJson, char** pcOut, uint32_t* puSizeOut, uint32_t* puLength)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(pcOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
        {
            PL_ASSERT(*puLength >= ptJson->sbuValueLength[i]);
            memset(pcOut[i], 0, *puLength);
            strncpy(pcOut[i],&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], ptJson->sbuValueLength[i]);
        }
    }
    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(puLength)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
        {
            if(ptJson->sbuValueLength[i] > *puLength)
                *puLength = ptJson->sbuValueLength[i];
        }       
    }
}

void
pl_json_as_bool_array(plJsonObject* ptJson, bool* pbOut, uint32_t* puSizeOut)
{
    PL_ASSERT(ptJson->tType == PL_JSON_TYPE_ARRAY);

    if(ptJson->tType != PL_JSON_TYPE_ARRAY)
        return;

    if(puSizeOut)
        *puSizeOut = ptJson->uChildCount;

    if(pbOut)
    {
        for(uint32_t i = 0; i < ptJson->uChildCount; i++)
            pbOut[i] = (&ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]])[0] == 't';
    }
}

void
pl_json_add_int_member(plJsonObject* ptJson, const char* pcName, int iValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%i", iValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%i", iValue);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = ' ';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint32_member(plJsonObject* ptJson, const char* pcName, uint32_t uValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%u", uValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%u", uValue);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = ' ';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint64_member(plJsonObject* ptJson, const char* pcName, uint64_t uValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%" PRIu64, uValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%" PRIu64, uValue);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = ' ';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_float_member(plJsonObject* ptJson, const char* pcName, float fValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%0.9g", fValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%0.9g", fValue);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = ' ';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_double_member(plJsonObject* ptJson, const char* pcName, double dValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_NUMBER;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%0.17g", dValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, "%0.17g", dValue);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = ' ';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_bool_member(plJsonObject* ptJson, const char* pcName, bool bValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_BOOL;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = snprintf(NULL, 0, "%s", bValue ? "true" : "false");
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    snprintf(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], tNewJsonObject.uValueLength + 1, bValue ? "true" : "false");
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_string_member(plJsonObject* ptJson, const char* pcName, const char* pcValue)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_STRING;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
    tNewJsonObject.uValueLength = (uint32_t)strlen(pcValue);
    pl_sb_resize(ptJson->ptRootObject->sbcBuffer, tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength + 1);
    memcpy(&ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset], pcValue, tNewJsonObject.uValueLength);
    ptJson->ptRootObject->sbcBuffer[tNewJsonObject.uValueOffset + tNewJsonObject.uValueLength] = '\0';
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

plJsonObject*
pl_json_add_member(plJsonObject* ptJson, const char* pcName)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    pl_sb_add_n(ptJson->sbtChildren, 1); 
    plJsonObject* ptResult = &pl_sb_top(ptJson->sbtChildren);
    memset(ptResult, 0, sizeof(plJsonObject));
    snprintf(ptResult->acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    ptResult->tType = PL_JSON_TYPE_OBJECT;
    ptResult->ptRootObject = ptJson->ptRootObject;
    return ptResult;
}

plJsonObject*
pl_json_add_member_array(plJsonObject* ptJson, const char* pcName, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;
    ptJson->tArrayElementType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    tNewJsonObject.sbtChildren = NULL;
    pl_sb_resize(tNewJsonObject.sbtChildren, uSize);
    for(uint32_t i = 0; i < uSize; i++)
        tNewJsonObject.sbtChildren[i].ptRootObject = ptJson->ptRootObject;
    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
    return &pl_sb_top(ptJson->sbtChildren);
}

void
pl_json_add_int_array(plJsonObject* ptJson, const char* pcName, const int* piValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%i", piValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%i", piValues[i]);

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint32_array(plJsonObject* ptJson, const char* pcName, const uint32_t* puValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%u", puValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%u", puValues[i]);

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_uint64_array(plJsonObject* ptJson, const char* pcName, const uint64_t* puValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%" PRIu64, puValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%" PRIu64, puValues[i]);

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_float_array(plJsonObject* ptJson, const char* pcName, const float* pfValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%0.9g", pfValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%0.9g", pfValues[i]);
    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_double_array(plJsonObject* ptJson, const char* pcName, const double* pdValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%0.17g", pdValues[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%0.17g", pdValues[i]);

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_bool_array(plJsonObject* ptJson, const char* pcName, const bool* pbValues, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%s", pbValues[i] ? "true" : "false");
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%s", pbValues[i] ? "true" : "false");

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

void
pl_json_add_string_array(plJsonObject* ptJson, const char* pcName, const char** ppcBuffer, uint32_t uSize)
{
    ptJson->uChildCount++;
    ptJson->uChildrenFound++;
    ptJson->tType = PL_JSON_TYPE_OBJECT;

    plJsonObject tNewJsonObject = {0};
    tNewJsonObject.tType = PL_JSON_TYPE_ARRAY;
    tNewJsonObject.tArrayElementType = PL_JSON_TYPE_STRING;
    tNewJsonObject.ptRootObject = ptJson->ptRootObject;
    snprintf(tNewJsonObject.acName, PL_JSON_MAX_NAME_LENGTH, "%s", pcName);
    tNewJsonObject.sbcBuffer = NULL;
    tNewJsonObject.uChildCount = uSize;
    tNewJsonObject.uChildrenFound = uSize;
    tNewJsonObject.sbuValueLength = NULL;
    tNewJsonObject.sbuValueOffsets = NULL;
    pl_sb_resize(tNewJsonObject.sbuValueLength, uSize);
    pl_sb_resize(tNewJsonObject.sbuValueOffsets, uSize);

    for(uint32_t i = 0; i < uSize; i++)
    {
        const uint32_t uValueOffset = pl_sb_size(ptJson->ptRootObject->sbcBuffer);
        const uint32_t uValueLength = snprintf(NULL, 0, "%s", ppcBuffer[i]);
        tNewJsonObject.sbuValueOffsets[i] = uValueOffset;
        tNewJsonObject.sbuValueLength[i] = uValueLength;

        pl_sb_resize(ptJson->ptRootObject->sbcBuffer, uValueOffset + uValueLength + 1);
        snprintf(&ptJson->ptRootObject->sbcBuffer[uValueOffset], uValueLength + 1, "%s", ppcBuffer[i]);

    }

    pl_sb_push(ptJson->sbtChildren, tNewJsonObject);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plJsonType
pl__get_json_token_object_type(const char* pcJson, jsmntok_t* ptToken)
{
    switch (ptToken->type)
    {
    case JSMN_ARRAY:  return PL_JSON_TYPE_ARRAY;
    case JSMN_OBJECT: return PL_JSON_TYPE_OBJECT;
    case JSMN_STRING: return PL_JSON_TYPE_STRING;
    case JSMN_PRIMITIVE:
        if     (pcJson[ptToken->start] == 'n')                                      { return PL_JSON_TYPE_NULL;}
        else if(pcJson[ptToken->start] == 't' || pcJson[ptToken->start] == 'f') { return PL_JSON_TYPE_BOOL;}
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
            int iSizeNeeded = snprintf(NULL, 0, "%s", (&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
            snprintf(&pcBuffer[uCursorPosition], iSizeNeeded + 1, "%s", (&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
            uCursorPosition += iSizeNeeded;
            break;
        }

        case PL_JSON_TYPE_NUMBER:
        {
            memcpy(&pcBuffer[uCursorPosition], &ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset], ptJson->uValueLength);
            uCursorPosition += ptJson->uValueLength;
            break;
        }

        case PL_JSON_TYPE_STRING:
        {
            int iSizeNeeded = snprintf(&pcBuffer[uCursorPosition], (int)ptJson->uValueLength + 2 + 1, "\"%s\"", &ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset]);
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
                memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded3 - 1);
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
            if(ptJson->tArrayElementType == PL_JSON_TYPE_OBJECT)
            {
                int iSizeNeeded = 2 + *puDepth * 4;
                pcBuffer[uCursorPosition] = '[';
                pcBuffer[uCursorPosition + 1] = '\n';
                memset(&pcBuffer[uCursorPosition + 2], 0x20, iSizeNeeded - 2);
                uCursorPosition += iSizeNeeded;
            }
            else
            {
                int iSizeNeeded = 2;
                pcBuffer[uCursorPosition] = '[';
                memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded - 1);
                uCursorPosition += iSizeNeeded;
            }


            if(ptJson->sbuValueOffsets)
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {
                    if(ptJson->tArrayElementType == PL_JSON_TYPE_STRING)
                    {
                        int iSizeNeeded2 = ptJson->sbuValueLength[i] + 2;
                        snprintf(&pcBuffer[uCursorPosition], iSizeNeeded2 + 1, "\"%s\"", &ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]]);
                        uCursorPosition += iSizeNeeded2;
                    }
                    else
                    {
                        memcpy(&pcBuffer[uCursorPosition], &ptJson->ptRootObject->sbcBuffer[ptJson->sbuValueOffsets[i]], ptJson->sbuValueLength[i]);
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
                        int iSizeNeeded3 = *puDepth * 4 + 2;
                        pcBuffer[uCursorPosition] = ',';
                        pcBuffer[uCursorPosition + 1] = '\n';
                        memset(&pcBuffer[uCursorPosition + 2], 0x20, iSizeNeeded3 - 2);
                        uCursorPosition += iSizeNeeded3;
                    }
                }
            }
 
            *puDepth -= 1;

            if(ptJson->tArrayElementType == PL_JSON_TYPE_OBJECT)
            {
                int iSizeNeeded3 = *puDepth * 4 + 2;
                pcBuffer[uCursorPosition] = '\n';
                memset(&pcBuffer[uCursorPosition + 1], 0x20, iSizeNeeded3 - 2);
                uCursorPosition += iSizeNeeded3;
                pcBuffer[uCursorPosition - 1] = ']';
            }
            else
            {
                int iSizeNeeded3 = 2;
                memset(&pcBuffer[uCursorPosition], 0x20, iSizeNeeded3 - 1);
                pcBuffer[uCursorPosition + 1] = ']';
                uCursorPosition += iSizeNeeded3;
                
            }
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
            
            int iSizeNeeded = snprintf(NULL, 0, "%s", (&ptJson->ptRootObject->sbcBuffer[ptJson->uValueOffset])[0] == 't' ? "true" : "false");
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
            if(ptJson->tArrayElementType == PL_JSON_TYPE_OBJECT)
            {
                int iSizeNeeded = 2 + *puDepth * 4;
                uCursorPosition += iSizeNeeded;
            }
            else
            {
                int iSizeNeeded = 2;
                uCursorPosition += iSizeNeeded;
            }

            if(ptJson->sbuValueOffsets)
            {
                for(uint32_t i = 0; i < ptJson->uChildCount; i++)
                {

                    if(ptJson->tArrayElementType == PL_JSON_TYPE_STRING)
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
                    {
                        uCursorPosition += *puDepth * 4 + 2;
                    }
                }
            }
 
            *puDepth -= 1;
            if(ptJson->tArrayElementType == PL_JSON_TYPE_OBJECT)
            {
                int iSizeNeeded3 = *puDepth * 4 + 2;
                uCursorPosition += iSizeNeeded3;
            }
            else
            {
                uCursorPosition += 2;
            }
            break;
        }
    }

    *puBufferSize = uBufferSize;
    *puCursor = uCursorPosition;
}

void
pl_load_json_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plJsonI tApi = {
        .load = pl_json_load,
        .unload = pl_json_unload,
        .new_root_object = pl_json_new_root_object,
        .write = pl_json_write,
        .member_by_index = pl_json_member_by_index,
        .member_list = pl_json_member_list,
        .member_exist = pl_json_member_exist,
        .get_type = pl_json_get_type,
        .get_name = pl_json_get_name,
        .int_member = pl_json_int_member,
        .uint32_member = pl_json_uint32_member,
        .uint64_member = pl_json_uint64_member,
        .float_member = pl_json_float_member,
        .double_member = pl_json_double_member,
        .string_member = pl_json_string_member,
        .bool_member = pl_json_bool_member,
        .member = pl_json_member,
        .array_member = pl_json_array_member,
        .int_array_member = pl_json_int_array_member,
        .uint32_array_member = pl_json_uint32_array_member,
        .uint64_array_member = pl_json_uint64_array_member,
        .float_array_member = pl_json_float_array_member,
        .double_array_member = pl_json_double_array_member,
        .bool_array_member = pl_json_bool_array_member,
        .string_array_member = pl_json_string_array_member,
        .as_int = pl_json_as_int,
        .as_uint32 = pl_json_as_uint32,
        .as_uint64 = pl_json_as_uint64,
        .as_float = pl_json_as_float,
        .as_double = pl_json_as_double,
        .as_string = pl_json_as_string,
        .as_bool = pl_json_as_bool,
        .as_int_array = pl_json_as_int_array,
        .as_uint32_array = pl_json_as_uint32_array,
        .as_uint64_array = pl_json_as_uint64_array,
        .as_float_array = pl_json_as_float_array,
        .as_double_array = pl_json_as_double_array,
        .as_bool_array = pl_json_as_bool_array,
        .as_string_array = pl_json_as_string_array,
        .add_int_member = pl_json_add_int_member,
        .add_uint32_member = pl_json_add_uint32_member,
        .add_uint64_member = pl_json_add_uint64_member,
        .add_float_member = pl_json_add_float_member,
        .add_double_member = pl_json_add_double_member,
        .add_bool_member = pl_json_add_bool_member,
        .add_string_member = pl_json_add_string_member,
        .add_int_array = pl_json_add_int_array,
        .add_uint32_array = pl_json_add_uint32_array,
        .add_uint64_array = pl_json_add_uint64_array,
        .add_float_array = pl_json_add_float_array,
        .add_double_array = pl_json_add_double_array,
        .add_bool_array = pl_json_add_bool_array,
        .add_string_array = pl_json_add_string_array,
        .add_member = pl_json_add_member,
        .add_member_array = pl_json_add_member_array
    };
    pl_set_api(ptApiRegistry, plJsonI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    #endif
}

void
pl_unload_json_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plJsonI* ptApi = pl_get_api_latest(ptApiRegistry, plJsonI);
    ptApiRegistry->remove_api(ptApi);
}