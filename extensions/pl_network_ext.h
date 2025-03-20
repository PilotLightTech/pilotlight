/*
   pl_network_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_NETWORK_EXT_H
#define PL_NETWORK_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plNetworkI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plSocket             plSocket;            // opaque type (used by platform backends)
typedef struct _plNetworkAddress     plNetworkAddress;    // opaque type (used by platform backends)
typedef struct _plSocketReceiverInfo plSocketReceiverInfo;

// enums
typedef int plNetworkAddressFlags; // -> enum _plNetworkAddressFlags // Flags:
typedef int plSocketFlags;         // -> enum _plSocketFlags         // Flags:
typedef int plNetworkResult;       // -> enum _plNetworkResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plNetworkI
{

    // addresses
    plNetworkResult (*create_address) (const char* address, const char* service, plNetworkAddressFlags, plNetworkAddress** addressPtrOut);
    void            (*destroy_address)(plNetworkAddress**);

    // sockets: general
    void            (*create_socket) (plSocketFlags, plSocket** socketPtrOut);
    void            (*destroy_socket)(plSocket**);
    plNetworkResult (*bind_socket)   (plSocket*, plNetworkAddress*);
    
    // sockets: udp usually
    plNetworkResult (*send_socket_data_to) (plSocket*, plNetworkAddress*, const void* data, size_t, size_t* sentPtrSizeOut);
    plNetworkResult (*get_socket_data_from)(plSocket*, void* dataOut, size_t, size_t* recievedPtrSize, plSocketReceiverInfo*);

    // sockets: tcp usually
    plNetworkResult (*select_sockets)  (plSocket** sockets, bool* selectedSockets, uint32_t socketCount, uint32_t timeOutMilliSec);
    plNetworkResult (*connect_socket)  (plSocket*, plNetworkAddress*);
    plNetworkResult (*listen_socket)   (plSocket*);
    plNetworkResult (*accept_socket)   (plSocket*, plSocket** socketPtrOut);
    plNetworkResult (*get_socket_data) (plSocket*, void* dataOut, size_t, size_t* recievedPtrSize);
    plNetworkResult (*send_socket_data)(plSocket*, void* data, size_t, size_t* sentPtrSizeOut);

} plNetworkI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSocketReceiverInfo
{
    char acAddressBuffer[100];
    char acServiceBuffer[100];
} plSocketReceiverInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plNetworkAddressFlags
{
    PL_NETWORK_ADDRESS_FLAGS_NONE = 0,
    PL_NETWORK_ADDRESS_FLAGS_IPV4 = 1 << 0,
    PL_NETWORK_ADDRESS_FLAGS_IPV6 = 1 << 1,
    PL_NETWORK_ADDRESS_FLAGS_UDP  = 1 << 2,
    PL_NETWORK_ADDRESS_FLAGS_TCP  = 1 << 3,
};

enum _plSocketFlags
{
    PL_SOCKET_FLAGS_NONE         = 0,
    PL_SOCKET_FLAGS_NON_BLOCKING = 1 << 0,
};

enum _plNetworkResult
{
    PL_NETWORK_RESULT_FAIL    = 0,
    PL_NETWORK_RESULT_SUCCESS = 1
};

#endif // PL_NETWORK_EXT_H