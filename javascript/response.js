var response = (function() {
    'use strict';

    var response = {
        checkHeader: function(buffer) {
            /* TODO: do communication here :-) */
            var rec_view = new DataView(buffer);
            var buf_pos = 0;

            /* Parse header */
            var version = rec_view.getUint8(buf_pos) >> 4;
            buf_pos += 2;

            if (version != 1) {
                return false;
            }

            return true;

        },

        parse: function(buffer) {
            var rec_view = new DataView(buffer);
            var buf_pos = 2;

            var message_len = rec_view.getUint16(buf_pos);
            buf_pos += 2;
            
            var op_code = rec_view.getUint8(buf_pos);
            buf_pos += 1;
            if (op_code == 8) { /* Is it command usr_auth_fail */
                var cmd_len = rec_view.getUint8(buf_pos);
                buf_pos += 1;
                if (cmd_len > 2) {
                    /* TODO: proccess all supported methods in loop and
                     * check cmd_len with message.len (should be smaller) */
                    var method = rec_view.getUint8(buf_pos);
                    buf_pos += 1;

                    if (method == 2) { /* Password method */
                        return 'passwd';
                    }
                } else {
                    /* TODO: end connection */
                    return;
                }
            }
        }
    };

    return response;

}());
