#include "control_protocol.h"

#include "control_verbs.h"

void control_request_release(control_request *request)
{
    if (request == NULL) {
        return;
    }
    control_framing_release_payload(&request->payload, &request->payload_size);
}

bool control_protocol_parse_request(
    const char *line,
    control_request *out_request,
    control_response *out_error)
{
    return apple2_control_parse_line(line, out_request, out_error);
}
