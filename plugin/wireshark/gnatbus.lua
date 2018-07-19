-- dissects the GnatBus Protocol
packetnum = -1
gnatbus_proto = Proto("GnatBus","GnatBus","GnatBus Protocol")

function dissects_register(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Register Request")
    ionumber = buffer(348,4):le_uint()


    subtree_header:add_le(buffer(16,4),"Bus version: (".. buffer(16,4):le_uint() .. ")")
    subtree_header:add_le(buffer(20,4),"Vendor Id: (" .. buffer(20,4):le_uint() .. ")")
    subtree_header:add_le(buffer(24,4),"Device Id: (" .. buffer(28,4):le_uint() .. ")")
    subtree_header:add(buffer(28,64),"Device name : '" .. buffer(28,64):string() .. "'")
    subtree_header:add(buffer(92,256),"Description: '" .. buffer(92,256):string() .. "'")
    subtree_header:add_le(buffer(348,4),"Number of I/O Mem: (".. ionumber .. ")")

    for i=0,(ionumber - 1) do
        -- Should be uint64
        subtree_header:add_le(buffer(352 + i * 16, 8), i .. " - I/O Mem base: (" .. buffer(352 + i * 16, 4):le_uint() .. ")")
        -- Should be uint64
        subtree_header:add_le(buffer(352 + i * 16 + 8, 8), "    I/O Mem size: (" .. buffer(352 + i * 16 + 8, 4):le_uint() .. ")")
    end
end


function dissects_read(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Read Request")
    subtree_header:add_le(buffer(16,8),"Address: (".. buffer(16,4):le_uint() .. ")")
    subtree_header:add_le(buffer(24,4),"Length: (" .. buffer(24,4):le_uint() .. ")")
end

function dissects_write(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Write Request")
    subtree_header:add_le(buffer(16,8),"Address: (".. buffer(16,4):le_uint() .. ")")
    subtree_header:add_le(buffer(24,4),"Length: (" .. buffer(24,4):le_uint() .. ")")
end


function dissects_request(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "GnatBus Request")
    type = buffer(8,4):le_uint()
    id = buffer(12,4):le_uint()

    type_str = "unkown"
    if type == 0 then
        type_str = "Read"
    elseif type == 1 then
        type_str = "Write"
    elseif type == 2 then
        type_str = "Register"
    elseif type == 3 then
        type_str = "Init"
    elseif type == 4 then
        type_str = "Reset"
    elseif type == 5 then
        type_str = "GetTime"
    end

    subtree_header:add_le(buffer(8,4),"type: ".. type_str .." (" .. type ..")")
    subtree_header:add_le(buffer(12,4),"Request ID: " .. id)
    pinfo.cols.info:set(type_str .. " Request")

    if type == 0 then
        dissects_read(buffer, pinfo, tree)
    elseif type == 1 then
        dissects_write(buffer, pinfo, tree)
    elseif type == 2 then
        dissects_register(buffer, pinfo, tree)
    end
end

function dissects_error(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Error Response")
    type = buffer(16,4):le_uint()

    if type == 0 then
        type_str = "Success"
    else
        type_str = "Error"
    end

    subtree_header:add_le(buffer(16,4),"Error Code: (".. buffer(16,4):le_uint() .. ")")
    pinfo.cols.info:set(type_str .. " Response")
end

function dissects_data(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Data Response")
    subtree_header:add_le(buffer(16,4),"Length: (".. buffer(16,4):le_uint() .. ")")
    pinfo.cols.info:set("Data Response")
end

function dissects_time(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "Time Response")
    -- Should be uint64
    subtree_header:add_le(buffer(16,4),"Time: (".. buffer(16,4):le_uint() .. ")")
    pinfo.cols.info:set("Time Response")
end

function dissects_response(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "GnatBus Response")
    type = buffer(8,4):le_uint()
    id = buffer(12,4):le_uint()

    type_str = "unkown"
    if type == 0 then
        type_str = "Error"
    elseif type == 1 then
        type_str = "Data"
    elseif type == 2 then
        type_str = "Time"
    end

    subtree_header:add_le(buffer(8,4),"type: ".. type_str .." (" .. type ..")")
    subtree_header:add_le(buffer(12,4),"Request ID: " .. id)

    if type == 0 then
        dissects_error(buffer, pinfo, tree)
    elseif type == 1 then
        dissects_data(buffer, pinfo, tree)
    elseif type == 2 then
        dissects_time(buffer, pinfo, tree)
    end
end

function dissects_setirq(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "SetIRQ Event")
    subtree_header:add_le(buffer(12,1),"Line: (".. buffer(12,1):le_uint() .. ")")

    type = buffer(13,1):le_uint()
    type_str = "unkown"
    if type == 0 then
        type_str = "Low"
    elseif type == 1 then
        type_str = "High"
    elseif type == 2 then
        type_str = "Pulse"
    end

    subtree_header:add_le(buffer(13,1),"Level: " .. type_str .. " (".. buffer(13,1):le_uint() .. ")")
end

function dissects_event(buffer,pinfo,tree)
    local subtree_header = tree:add_le(buffer(0,8), "GnatBus Event")
    type = buffer(8,4):le_uint()

    type_str = "unkown"
    if type == 0 then
        type_str = "Exit"
    elseif type == 1 then
        type_str = "SetIRQ"
    elseif type == 2 then
        type_str = "RegisterEvent"
    elseif type == 3 then
        type_str = "TriggerEvent"
    end

    subtree_header:add_le(buffer(8,4),"type: ".. type_str .." (" .. type ..")")
    pinfo.cols.info:set(type_str .. " Event")

    if type == 1 then
        dissects_setirq(buffer, pinfo, tree);
    end
end

-- create a function to dissect it
function gnatbus_proto.dissector(buffer,pinfo,tree)
    pinfo.cols.protocol = "GnatBus"
    cmd = buffer(4,4):le_uint()

    packetnum = pinfo.number

    local subtree_packet = tree:add(gnatbus_proto,buffer(),"GnatBus ("..buffer:len()..")")

    local subtree_header = subtree_packet:add_le(buffer(0,8), "GnatBus Header (8)")

    cmd_str = "unkown"
    if cmd == 0 then
        cmd_str = "Event"
    elseif cmd == 1 then
        cmd_str = "Request"
    elseif cmd == 2 then
        cmd_str = "Response"
    end

    datasize = buffer(0,4):le_uint()

    pinfo.cols.info:set(cmd_str)
    subtree_header:add_le(buffer(4,4),"type: ".. cmd_str .." (" .. cmd..")")
    subtree_header:add_le(buffer(0,4),"size: " .. datasize)

    if cmd == 0 then
       dissects_event(buffer, pinfo, subtree_packet);
    elseif cmd == 1 then
       dissects_request(buffer, pinfo, subtree_packet);
    elseif cmd == 2 then
       dissects_response(buffer, pinfo, subtree_packet);
    end
end

-- load the tcp.port table
tcp_table = DissectorTable.get("tcp.port")
-- register our protocol to handle tcp port 8032
tcp_table:add(8032,gnatbus_proto)
