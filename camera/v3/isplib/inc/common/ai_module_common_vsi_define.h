#ifndef NEUSDK_AI_MODULE_COMMON_VSI_DEFINE_H
#define NEUSDK_AI_MODULE_COMMON_VSI_DEFINE_H

// this file include part of the define of VSI, it is used if  we don't use VSI NNA
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t  vsi_status;
typedef uint32_t vx_uint32;
typedef int8_t   vx_int8;

enum vx_status_e {
    VX_STATUS_MIN                       = -25,/*!< \brief Indicates the lower bound of status codes in VX. Used for bounds checks only. */
    /* add new codes here */
    VX_ERROR_REFERENCE_NONZERO          = -24,/*!< \brief Indicates that an operation did not complete due to a reference count being non-zero. */
    VX_ERROR_MULTIPLE_WRITERS           = -23,/*!< \brief Indicates that the graph has more than one node outputting to the same data object. This is an invalid graph structure. */
    VX_ERROR_GRAPH_ABANDONED            = -22,/*!< \brief Indicates that the graph is stopped due to an error or a callback that abandoned execution. */
    VX_ERROR_GRAPH_SCHEDULED            = -21,/*!< \brief Indicates that the supplied graph already has been scheduled and may be currently executing. */
    VX_ERROR_INVALID_SCOPE              = -20,/*!< \brief Indicates that the supplied parameter is from another scope and cannot be used in the current scope. */
    VX_ERROR_INVALID_NODE               = -19,/*!< \brief Indicates that the supplied node could not be created.*/
    VX_ERROR_INVALID_GRAPH              = -18,/*!< \brief Indicates that the supplied graph has invalid connections (cycles). */
    VX_ERROR_INVALID_TYPE               = -17,/*!< \brief Indicates that the supplied type parameter is incorrect. */
    VX_ERROR_INVALID_VALUE              = -16,/*!< \brief Indicates that the supplied parameter has an incorrect value. */
    VX_ERROR_INVALID_DIMENSION          = -15,/*!< \brief Indicates that the supplied parameter is too big or too small in dimension. */
    VX_ERROR_INVALID_FORMAT             = -14,/*!< \brief Indicates that the supplied parameter is in an invalid format. */
    VX_ERROR_INVALID_LINK               = -13,/*!< \brief Indicates that the link is not possible as specified. The parameters are incompatible. */
    VX_ERROR_INVALID_REFERENCE          = -12,/*!< \brief Indicates that the reference provided is not valid. */
    VX_ERROR_INVALID_MODULE             = -11,/*!< \brief This is returned from <tt>\ref vxLoadKernels</tt> when the module does not contain the entry point. */
    VX_ERROR_INVALID_PARAMETERS         = -10,/*!< \brief Indicates that the supplied parameter information does not match the kernel contract. */
    VX_ERROR_OPTIMIZED_AWAY             = -9,/*!< \brief Indicates that the object refered to has been optimized out of existence. */
    VX_ERROR_NO_MEMORY                  = -8,/*!< \brief Indicates that an internal or implicit allocation failed. Typically catastrophic. After detection, deconstruct the context. \see vxVerifyGraph. */
    VX_ERROR_NO_RESOURCES               = -7,/*!< \brief Indicates that an internal or implicit resource can not be acquired (not memory). This is typically catastrophic. After detection, deconstruct the context. \see vxVerifyGraph. */
    VX_ERROR_NOT_COMPATIBLE             = -6,/*!< \brief Indicates that the attempt to link two parameters together failed due to type incompatibility. */
    VX_ERROR_NOT_ALLOCATED              = -5,/*!< \brief Indicates to the system that the parameter must be allocated by the system.  */
    VX_ERROR_NOT_SUFFICIENT             = -4,/*!< \brief Indicates that the given graph has failed verification due to an insufficient number of required parameters, which cannot be automatically created. Typically this indicates required atomic parameters. \see vxVerifyGraph. */
    VX_ERROR_NOT_SUPPORTED              = -3,/*!< \brief Indicates that the requested set of parameters produce a configuration that cannot be supported. Refer to the supplied documentation on the configured kernels. \see vx_kernel_e. This is also returned if a function to set an attribute is called on a Read-only attribute.*/
    VX_ERROR_NOT_IMPLEMENTED            = -2,/*!< \brief Indicates that the requested kernel is missing. \see vx_kernel_e vxGetKernelByName. */
    VX_FAILURE                          = -1,/*!< \brief Indicates a generic error code, used when no other describes the error. */
    VX_SUCCESS                          =  0,/*!< \brief No error. */
};

typedef int vx_status;// (VX_CALLBACK *vx_kernel_image_valid_rectangle_f)(vx_node node, vx_uint32 index, const vx_rectangle_t* const input_valid[], vx_rectangle_t* const output_valid[]);

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/** Status enum */
typedef enum
{
    VSI_FAILURE = VX_FAILURE,
    VSI_SUCCESS = VX_SUCCESS,
}vsi_nn_status_e;

#define _CHECK_PTR( ptr, lbl )      do {\
    if( NULL == ptr ) {\
    printf("%s CHECK PTR %d\n", __FUNCTION__, __LINE__);\
    goto lbl;\
    }\
} while(0)

#define _CHECK_STATUS( stat, lbl )  do {\
    if( VX_SUCCESS != stat ) {\
    printf("%s CHECK STATUS %d\n", __FUNCTION__, __LINE__);\
    goto lbl;\
    }\
} while(0)

#define _CHECK_INVALID(stat, value, lbl) do {\
    if (value == stat) { \
    printf("%s CHECK INVALID %d\n", __FUNCTION__, __LINE__); \
    goto lbl; \
    }\
} while(0)

#ifdef __cplusplus
}
#endif

#endif
