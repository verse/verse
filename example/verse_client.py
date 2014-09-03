#!/usr/bin/python3
#
# ***** BEGIN BSD LICENSE BLOCK *****
#
# Copyright (c) 2009-2012, Jiri Hnidek
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ***** END BSD LICENSE BLOCK *****
#
# Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
#


"""
Example of Verse client implemented in Python language
"""


import verse as vrs
import time
import getpass
import sys

class MyLayer():
    """Class representing layer"""
    def __init__(self, parent_layer, layer_id, data_type, count, custom_type):
        self.layer_id = layer_id
        self.parent_layer = parent_layer
        self.data_type = data_type
        self.count = count
        self.custom_type = custom_type
        self.child_layers = {}
        self.values = {}
        if parent_layer != None:
            parent_layer.add_child_layer(self)
        
    def add_child_layer(self, child_layer):
        """Add another child layer to this layer"""
        self.child_layers[child_layer.id] = child_layer
    
    def unset_value(self, item_id):
        """Unset method in this layer and all child layers"""
        # Try to unset value
        try:
            self.values.pop(item_id)
        except KeyError:
            pass
        # Unset value in child layers too
        for child_layer in self.child_layers.values():
            child_layer.unset_value(item_id)
    

class MyTag():
    """Class representing tag"""
    def __init__(self, taggroup, tag_id, data_type, count, custom_type):
        self.tag_id = tag_id
        self.taggroup = taggroup
        self.data_type = data_type
        self.count = count
        self.custom_type = custom_type
        self.value = []

class MyTagGroup():
    """Class representing tag group"""
    def __init__(self, node, taggroup_id, custom_type):
        self.taggroup_id = taggroup_id
        self.node = node
        self.custom_type = custom_type
        self.tags = {}

class MyNode():
    """Class representating verse node"""
    def __init__(self, node_id, parent, user_id, custom_type):
        self.node_id = node_id
        self.parent = parent
        self.user_id = user_id
        self.custom_type = custom_type
        self.taggroups = {}
        self.layers = {}
        self.locker = None
        self.perms = {}

class MySession(vrs.Session):
    """Session Class for this Python verse client"""
    
    def __init__(self, hostname, service, flags):
        # Call __init__ from parent class to connect to verse server
        super(MySession, self).__init__(hostname, service, flags)
        # Set initial state and settings
        self.state = "CONNECTING"
        self.fps = 30
        # Create dictionary of nodes
        self.nodes = {}
        # User and Client
        self.user_id = None
        self.avatar_id = None
        # My testing attributes
        self.test_node = None
        self.test_taggroup = None
        self.test_string_tag = None
        #self.test_child_layer = None
        #self.test_parent_layer = None
        
    def _receive_connect_terminate(self, error):
        """Callback function for connect accept"""
        print("MY connect_terminate(): ",
              "error: ", error)
        self.state = "DISCONNECTED"
        # Clear dictionary of nodes
        self.nodes.clear()

    def _receive_connect_accept(self, user_id, avatar_id):
        """Callback function for connect accept"""
        out_queue_size = self.get(vrs.SESSION_OUT_QUEUE_MAX_SIZE)
        print("MY connect_accept(): ",
              "user_id: ", user_id,
              ", avatar_id: ", avatar_id,
              ", out queue size: ", out_queue_size)
        self.state = "CONNECTED"
        self.user_id = user_id
        self.avatar_id = avatar_id
        # Send used FPS by this client to the server
        self.send_fps(prio=vrs.DEFAULT_PRIORITY, fps=self.fps)
        # Add root node to the dictionary of nodes
        self.nodes[0] = MyNode(0, None, 0, 0)
        # Subscribe to the root of node tree
        self.send_node_subscribe(prio=vrs.DEFAULT_PRIORITY, node_id=0, version=0, crc32=0)
        # Create my new node
        self.send_node_create(prio=vrs.DEFAULT_PRIORITY, custom_type=32)
        # Try to decrease size of outgoing queue a little
        self.set(vrs.SESSION_OUT_QUEUE_MAX_SIZE, out_queue_size - 576)
        # Print new queue size
        print("    new queue size: ", self.get(vrs.SESSION_OUT_QUEUE_MAX_SIZE))

    def _receive_user_authenticate(self, username, methods):
        """Callback function for user authenticate"""
        print("MY user_authenticate(): ",
              "username: ", username,
              ", methods: ", methods)
        if username == "":
            if sys.version > '3':
                username = __builtins__.input('Username: ')
            else:
                username = __builtins__.raw_input('Username: ')
            self.send_user_authenticate(username, vrs.UA_METHOD_NONE, "")
        else:
            if methods.count(vrs.UA_METHOD_PASSWORD)>=1:
                password = getpass.getpass('Password:')
                self.send_user_authenticate(username, vrs.UA_METHOD_PASSWORD, password)
            else:
                print("Unsuported authenticate method")

    def _receive_node_create(self, node_id, parent_id, user_id, custom_type):
        """Callback function for node create"""
        print("MY node_create(): ",
              "node_id: ", node_id,
              ", parent_node_id: ", parent_id,
              ", user_id: ", user_id,
              ", custom_type: ", custom_type)
        # Subscribe to node
        self.send_node_subscribe(vrs.DEFAULT_PRIORITY, node_id, 0, 0)
        # Try to find parent node
        try:
            parent_node = self.nodes[parent_id]
        except KeyError:
            parent_node = None
        # Add node to the dictionary of nodes
        self.nodes[node_id] = MyNode(node_id, parent_node, user_id, custom_type)
        # Do some test with my new node
        if parent_id == self.avatar_id:
            self.send_node_link(vrs.DEFAULT_PRIORITY, 3, node_id)
            self.test_node = self.nodes[node_id]
            self.send_taggroup_create(vrs.DEFAULT_PRIORITY, node_id, 0)
            self.send_layer_create(vrs.DEFAULT_PRIORITY, node_id, -1, vrs.VALUE_TYPE_UINT8, 3, 1)
    
    def _receive_node_destroy(self, node_id):
        """Callback function for node destroy"""
        print("MY node_destroy(): ",
              "node_id: ", node_id)
        # Remove node from list of nodes
        try:
            self.nodes.pop(node_id)
        except KeyError:
            pass
    
    def _receive_node_link(self, parent_node_id, child_node_id):
        """Callback function for node link"""
        print("MY node_link(): ",
              "parent_node_id: ", parent_node_id,
              " ,child_node_id: ", child_node_id)
        try:
            parent_node = self.nodes[parent_node_id]
        except KeyError:
            parent_node = None
        try:
            child_node = self.nodes[child_node_id]
        except KeyError:
            return
        child_node.parent = parent_node
        
    def _receive_node_perm(self, node_id, user_id, perm):
        """Callback function for node perm"""
        print("MY node_perm(): ",
              "node_id: ", node_id,
              " ,user_id: ", user_id,
              " ,perm: ", perm)
        try:
            node = self.nodes[node_id]
        except KeyError:
            return
        node.perms[user_id] = perm
    
    def _receive_node_owner(self, node_id, user_id):
        """Callback function for node owner"""
        print("MY node_owner(): ",
              "node_id: ", node_id,
              " ,user_id: ", user_id)
        try:
            node = self.nodes[node_id]
        except KeyError:
            return
        node.user_id = user_id
    
    def _receive_node_lock(self, node_id, avatar_id):
        """Callback function for node lock"""
        print("MY node_lock(): ",
              "node_id: ", node_id,
              " ,avatar_id: ", avatar_id)
        try:
            node = self.nodes[node_id]
        except KeyError:
            return
        node.locker = avatar_id
        
    def _receive_node_unlock(self, node_id, avatar_id):
        """Callback function for node unlock"""
        print("MY node_unlock(): ",
              "node_id: ", node_id,
              " ,avatar_id: ", avatar_id)
        try:
            node = self.nodes[node_id]
        except KeyError:
            return
        node.locker = None
            
    def _receive_taggroup_create(self, node_id, taggroup_id, custom_type):
        """Callback function for taggroup create"""
        print("MY taggroup_create(): ",
              "node_id: ", node_id,
              " ,taggroup_id: ", taggroup_id,
              " ,custom_type: ", custom_type)
        self.send_taggroup_subscribe(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, 0, 0)
        self.nodes[node_id].taggroups[taggroup_id] = MyTagGroup(self.nodes[node_id], taggroup_id, custom_type)
        if self.nodes[node_id] == self.test_node:
            self.test_taggroup = self.test_node.taggroups[taggroup_id]
            self.send_tag_create(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, vrs.VALUE_TYPE_UINT8, 3, 1)
            self.send_tag_create(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, vrs.VALUE_TYPE_UINT8, 1, 2)
            self.send_tag_create(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, vrs.VALUE_TYPE_REAL32, 1, 3)
            self.send_tag_create(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, vrs.VALUE_TYPE_STRING8, 1, 4)

    def _receive_taggroup_destroy(self, node_id, taggroup_id):
        """Callback function for taggroup destroy"""
        print("MY taggroup_destroy(): ",
              "node_id: ", node_id,
              " ,taggroup_id: ", taggroup_id)
        try:
            self.nodes[node_id].taggroups.pop(taggroup_id)
        except KeyError:
            pass
    
    def _receive_tag_create(self, node_id, taggroup_id, tag_id, data_type, count, custom_type):
        """Callback function for tag create"""
        print("MY tag_create(): ",
              "node_id: ", node_id,
              " ,taggroup_id: ", taggroup_id,
              " ,tag_id: ", tag_id,
              " ,data_type: ", data_type,
              " ,count: ", count,
              " ,custom_type: ", custom_type)
        taggroup = self.nodes[node_id].taggroups[taggroup_id]
        tag = MyTag(taggroup, tag_id, data_type, count, custom_type)
        taggroup.tags[tag_id] = tag
        if self.test_taggroup == taggroup and data_type == vrs.VALUE_TYPE_UINT8 and custom_type == 1:
            self.test_tuple_tag = tag
            tag.value = (123, 124, 125)
            self.send_tag_set_values(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, tag_id, tag.data_type, tag.value)
        elif self.test_taggroup == taggroup and data_type == vrs.VALUE_TYPE_UINT8 and custom_type == 2:
            self.test_int_tag = tag
            tag.value = (10,)
            self.send_tag_set_values(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, tag_id, tag.data_type, tag.value)
        elif self.test_taggroup == taggroup and data_type == vrs.VALUE_TYPE_REAL32 and custom_type == 3:
            self.test_float_tag = tag
            tag.value = (12.345,)
            self.send_tag_set_values(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, tag_id, tag.data_type, tag.value)
        elif self.test_taggroup == taggroup and data_type == vrs.VALUE_TYPE_STRING8 and custom_type == 4:
            self.test_string_tag = tag
            tag.value = ('Ahoj',)
            self.send_tag_set_values(vrs.DEFAULT_PRIORITY, node_id, taggroup_id, tag_id, tag.data_type, tag.value)
    
    def _receive_tag_destroy(self, node_id, taggroup_id, tag_id):
        """Callback function for tag destroy"""
        print("MY tag_destroy(): ",
              "node_id: ", node_id,
              " ,taggroup_id: ", taggroup_id,
              " ,tag_id: ", tag_id)
        try:
            self.nodes[node_id].taggroups[taggroup_id].tags.pop(tag_id)
        except KeyError:
            pass
    
    def _receive_tag_set_values(self, node_id, taggroup_id, tag_id, values):
        """Callback function for tag set value"""
        print("MY tag_set_values(): ",
              "node_id: ", node_id,
              " ,taggroup_id: ", taggroup_id,
              " ,tag_id: ", tag_id,
              " ,values: ", values)
        self.nodes[node_id].taggroups[taggroup_id].tags[tag_id].value = list(values)
    
    def _receive_layer_create(self, node_id, parent_layer_id, layer_id, data_type, count, custom_type):
        """Callback function for layer create"""
        print("MY layer_create(): ",
              "node_id: ", node_id,
              " ,parent_layer_id: ", parent_layer_id,
              " ,layer_id: ", layer_id,
              " ,data_type: ", data_type,
              " ,count: ", count,
              " ,custom_type: ", custom_type)
        # Subscribe to the layer
        self.send_layer_subscribe(vrs.DEFAULT_PRIORITY, node_id, layer_id, 0, 0)
        try:
            parent_layer = self.nodes[node_id].layers[parent_layer_id]
        except KeyError:
            parent_layer = None
        self.nodes[node_id].layers[layer_id] = MyLayer(parent_layer, layer_id, data_type, count, custom_type)
    
    def _receive_layer_destroy(self, node_id, layer_id):         
        """Callback function for layer destroy"""
        print("MY layer_destroy(): ",
              "node_id: ", node_id,
              " ,layer_id: ", layer_id)
        try:
            self.nodes[node_id].layers.pop(layer_id)
        except KeyError:
            pass
    
    def _receive_layer_set_value(self, node_id, layer_id, item_id, values):
        """Callback function for layer set value"""
        print("MY layer_set_values(): ",
              "node_id: ", node_id,
              " ,layer_id: ", layer_id,
              " ,item_id: ", layer_id,
              " ,values: ", values)
        self.nodes[node_id].layers[layer_id].values[item_id] = values
    
    def _receive_layer_unset_value(self, node_id, layer_id, item_id):
        """Callback function for layer set value"""
        print("MY layer_unset_values(): ",
              "node_id: ", node_id,
              " ,layer_id: ", layer_id,
              " ,item_id: ", item_id)
        try:
            layer = self.nodes[node_id].layers[layer_id]
        except KeyError:
            layer = None
        if layer != None:
            layer.unset_value(item_id)
        
            
def main(hostname='localhost'):
    """Main function of this client"""
    received_signal = False
    
    # Set debug level
    vrs.set_debug_level(vrs.PRINT_DEBUG_MSG)
    
    # Set name and version of client
    vrs.set_client_info("Example Python Verse Client", "0.1")
    
    try:
        session = MySession(hostname, "12345", vrs.DGRAM_SEC_NONE)
    except vrs.VerseError:
        return
    
    while session.state != "DISCONNECTED":
        try:
            session.callback_update()
        except vrs.VerseError:
            print("Error: callback_update")
            session = None
        try:
            time.sleep(1.0/float(session.fps))
        except KeyboardInterrupt:
            if received_signal == False:
                print("Terminating connection ...")
                session.send_connect_terminate()
                session.state = "DISCONNECTING"
                received_signal = True
            else:
                return
    else:
        del(session)

if __name__ == "__main__":
    import sys
    if sys.argv[0] != sys.argv[-1]:
        main(sys.argv[-1])
    else:
        main()
