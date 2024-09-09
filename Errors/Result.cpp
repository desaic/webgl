#include "Result.h"

#include "cerrno"

Errc ErrcFromInt(int errc_int) {
  Errc errno_as_errc = static_cast<Errc>(errc_int);
  switch (errno_as_errc) {
    case Errc::address_family_not_supported:
    case Errc::address_in_use:
    case Errc::address_not_available:
    case Errc::already_connected:
    case Errc::argument_list_too_long:
    case Errc::argument_out_of_domain:
    case Errc::bad_address:
    case Errc::bad_file_descriptor:
    case Errc::bad_message:
    case Errc::broken_pipe:
    case Errc::connection_aborted:
    case Errc::connection_already_in_progress:
    case Errc::connection_refused:
    case Errc::connection_reset:
    case Errc::cross_device_link:
    case Errc::destination_address_required:
    case Errc::device_or_resource_busy:
    case Errc::directory_not_empty:
    case Errc::executable_format_error:
    case Errc::file_exists:
    case Errc::file_too_large:
    case Errc::filename_too_long:
    case Errc::function_not_supported:
    case Errc::host_unreachable:
    case Errc::identifier_removed:
    case Errc::illegal_byte_sequence:
    case Errc::inappropriate_io_control_operation:
    case Errc::interrupted:
    case Errc::invalid_argument:
    case Errc::invalid_seek:
    case Errc::io_error:
    case Errc::is_a_directory:
    case Errc::message_size:
    case Errc::network_down:
    case Errc::network_reset:
    case Errc::network_unreachable:
    case Errc::no_buffer_space:
    case Errc::no_child_process:
    case Errc::no_link:
    case Errc::no_lock_available:
    case Errc::no_message:
    case Errc::no_protocol_option:
    case Errc::no_space_on_device:
    case Errc::no_stream_resources:
    case Errc::no_such_device_or_address:
    case Errc::no_such_device:
    case Errc::no_such_file_or_directory:
    case Errc::no_such_process:
    case Errc::not_a_directory:
    case Errc::not_a_socket:
    case Errc::not_a_stream:
    case Errc::not_connected:
    case Errc::not_enough_memory:
#if ENOTSUP != EOPNOTSUPP
    case Errc::not_supported:
#endif
    case Errc::operation_canceled:
    case Errc::operation_in_progress:
    case Errc::operation_not_permitted:
    case Errc::operation_not_supported:
#if EAGAIN != EWOULDBLOCK
    case Errc::operation_would_block:
#endif
    case Errc::owner_dead:
    case Errc::permission_denied:
    case Errc::protocol_error:
    case Errc::protocol_not_supported:
    case Errc::read_only_file_system:
    case Errc::resource_deadlock_would_occur:
    case Errc::resource_unavailable_try_again:
    case Errc::result_out_of_range:
    case Errc::state_not_recoverable:
    case Errc::stream_timeout:
    case Errc::text_file_busy:
    case Errc::timed_out:
    case Errc::too_many_files_open_in_system:
    case Errc::too_many_files_open:
    case Errc::too_many_links:
    case Errc::too_many_symbolic_link_levels:
    case Errc::value_too_large:
    case Errc::wrong_protocol_type:
      return errno_as_errc;
    default:
      return Errc::unknown;
  }
}

Error Error::from_errno() { return ErrcFromInt(errno); }

const char* ErrcToString(Errc code) {
  switch (code) {
    // case Errc::success:
    //   return "Success";
    // clang-format off
    case Errc::address_family_not_supported: return "Address family not supported by protocol";
    case Errc::address_in_use: return "Address already in use";
    case Errc::address_not_available: return "Cannot assign requested address";
    case Errc::already_connected: return "Transport endpoint is already connected";
    case Errc::argument_list_too_long: return "Argument list too long";
    case Errc::argument_out_of_domain: return "Numerical argument out of domain";
    case Errc::bad_address: return "Bad address";
    case Errc::bad_file_descriptor: return "Bad file descriptor";
    case Errc::bad_message: return "Bad message";
    case Errc::broken_pipe: return "Broken pipe";
    case Errc::connection_aborted: return "Software caused connection abort";
    case Errc::connection_already_in_progress: return "Operation already in progress";
    case Errc::connection_refused: return "Connection refused";
    case Errc::connection_reset: return "Connection reset by peer";
    case Errc::cross_device_link: return "Invalid cross-device link";
    case Errc::destination_address_required: return "Destination address required";
    case Errc::device_or_resource_busy: return "Device or resource busy";
    case Errc::directory_not_empty: return "Directory not empty";
    case Errc::executable_format_error: return "Exec format error";
    case Errc::file_exists: return "File exists";
    case Errc::file_too_large: return "File too large";
    case Errc::filename_too_long: return "File name too long";
    case Errc::function_not_supported: return "Function not implemented";
    case Errc::host_unreachable: return "No route to host";
    case Errc::identifier_removed: return "Identifier removed";
    case Errc::illegal_byte_sequence: return "Invalid or incomplete multibyte or wide character";
    case Errc::inappropriate_io_control_operation: return "Inappropriate ioctl for device";
    case Errc::interrupted: return "Interrupted system call";
    case Errc::invalid_argument: return "Invalid argument";
    case Errc::invalid_seek: return "Illegal seek";
    case Errc::io_error: return "Input/output error";
    case Errc::is_a_directory: return "Is a directory";
    case Errc::message_size: return "Message too long";
    case Errc::network_down: return "Network is down";
    case Errc::network_reset: return "Network dropped connection on reset";
    case Errc::network_unreachable: return "Network is unreachable";
    case Errc::no_buffer_space: return "No buffer space available";
    case Errc::no_child_process: return "No child processes";
    case Errc::no_link: return "Link has been severed";
    case Errc::no_lock_available: return "No locks available";
    case Errc::no_message: return "No message of desired type";
    case Errc::no_protocol_option: return "Protocol not available";
    case Errc::no_space_on_device: return "No space left on device";
    case Errc::no_stream_resources: return "Out of streams resources";
    case Errc::no_such_device_or_address: return "No such device or address";
    case Errc::no_such_device: return "No such device";
    case Errc::no_such_file_or_directory: return "No such file or directory";
    case Errc::no_such_process: return "No such process";
    case Errc::not_a_directory: return "Not a directory";
    case Errc::not_a_socket: return "Socket operation on non-socket";
    case Errc::not_a_stream: return "Device not a stream";
    case Errc::not_connected: return "Transport endpoint is not connected";
    case Errc::not_enough_memory: return "Cannot allocate memory";
#if ENOTSUP != EOPNOTSUPP
    case Errc::not_supported: return "Operation not supported";
#endif
    case Errc::operation_canceled: return "Operation canceled";
    case Errc::operation_in_progress: return "Operation now in progress";
    case Errc::operation_not_permitted: return "Operation not permitted";
    case Errc::operation_not_supported: return "Operation not supported";
#if EAGAIN != EWOULDBLOCK
    case Errc::operation_would_block: return "Resource temporarily unavailable";
#endif
    case Errc::owner_dead: return "Owner died";
    case Errc::permission_denied: return "Permission denied";
    case Errc::protocol_error: return "Protocol error";
    case Errc::protocol_not_supported: return "Protocol not supported";
    case Errc::read_only_file_system: return "Read-only file system";
    case Errc::resource_deadlock_would_occur: return "Resource deadlock avoided";
    case Errc::resource_unavailable_try_again: return "Resource temporarily unavailable";
    case Errc::result_out_of_range: return "Numerical result out of range";
    case Errc::state_not_recoverable: return "State not recoverable";
    case Errc::stream_timeout: return "Timer expired";
    case Errc::text_file_busy: return "Text file busy";
    case Errc::timed_out: return "Connection timed out";
    case Errc::too_many_files_open_in_system: return "Too many open files in system";
    case Errc::too_many_files_open: return "Too many open files";
    case Errc::too_many_links: return "Too many links";
    case Errc::too_many_symbolic_link_levels: return "Too many levels of symbolic links";
    case Errc::value_too_large: return "Value too large for defined data type";
    case Errc::wrong_protocol_type: return "Protocol wrong type for socket";
    default: return "unknown";
      // clang-format on
  }
}
