--- @class Param
--- @field id integer The ID of the param.
--- @field node integer The node field description
--- @field type integer The type of the param.
--- @field length integer The length of the param if array.
--- @field cache table|integer|number set or get local cached param values. Will also set the set state of the parameter + index


---@class Param
local Param = {}

---@class ParamList
local ParamList = {}

---@class Vmem
local Vmem = {}


--- Set the remote parameters value.
--- @param self Param The instance of the userdata.
--- @param index? integer @(optional) index for param, default 0
--- @param value number|string|integer The value to set, type depends on the parameter type.
--- @throw if timeout or csp buffer error
function Param:set(index, value) end

--- Set the remote parameters value.
--- @param self Param The instance of the userdata.
--- @param value number|string|integer The value to set, type depends on the parameter type.
--- @throw if timeout or csp buffer error
function Param:set(value) end

--- Get the value of the remote parameter value.
--- @param self Param The instance of the userdata.
--- @param index? integer @(optional) index for param, default all
--- @return number|string value The value of the parameter.
--- @throw if timeout or csp buffer error
function Param:get(index) end

--- Get the value of the remote parameter value.
--- @param self Param The instance of the userdata.
--- @return number|string value All the values of the parameter.
--- @throw if timeout or csp buffer error
function Param:get() end

--- A function that returns a remote paramameter userdata.
--- If a parameter already exists in the lua state with id,node pair then
--- returns a pointer to the same remote parameter
--
--- @param id integer The id of the param.
--- @param node integer The remote node 
--- @param typestr string|integer The type of the param. Use the globals UINT8 etc. or string "u08"
--- @param array_len? integer @(optional) array length, defaults to 1 if string or data provide the correct size
--- @return Param
--- @throw if wrong args or create remote failed 
function parameter(id, node, typestr, array_len) end

--- Get the values of all the parameters in list.
--- Will reset the set state of the parameters if no timeout
--- @param self ParamList The instance of the userdata.
--- @throw if timeout
function ParamList:pull() end

--- Push all changed local values of parameters in parameter list.
--- Will request ack with pull which sets local values to what actually gets set
--- A timeout will not reset the set state of the parameters and will therefore still be pushed if pushed again
--- @param self ParamList The instance of the userdata.
--- @throw if timeout
function ParamList:push() end

--- Add parameter(s) to parameter list they need to be on the same remote node
--- @param self ParamList The instance of the userdata.
--- @param parameter Param | table parameter or table of parameters
--- @throw if wrong type arg
function ParamList:add(parameter) end

--- Remove parameter(s) from parameter list
--- @param self ParamList The instance of the userdata.
--- @param parameter Param | table parameter or table of parameters
--- @throw if wrong type arg
function ParamList:remove(parameter) end


--- Download a packed binary string from the specified node and address
--
--- @param self Vmem The instance of the userdata.
--- @param length integer Number of bytes to download
--- @param offset? Integer @(optional) @(default=0) Start reading from `self.address+offset`
--- @param use_rdp? integer @(optional) @(default=1) Bool integer
--- @return string A `string.unpack()` compatible binary string
--- @throw if timeout or csp buffer error
function Vmem:read(length, offset, use_rdp) end

--- Upload a packed binary string to the specified node and address
--
--- @param self Vmem The instance of the userdata.
--- @param data_str string Preferably a `string.pack()` binary string
--- @param offset? Integer @(optional) @(default=0) Start writing from `self.address+offset`
--- @return Integer >0 on failure, otherwise number of bytes uploaded
--- @throw if timeout or csp buffer error
function Vmem:write(data_str, offset) end

--- Constructor function to create a reusable `Vmem` area userdata.
--
--- @param node integer Node to `:read()` from and `:write()` to, may be 0 for local VMEM.
--- @param address integer Start address of the VMEM area. `:read()` and `:write()` may supply an offset.
--- @param use_peekpoke? integer @(optional) Whether to use peek/poke (without RDP) for read/writes. Will error if transfers cant fit in 1 CSP packet. Defaults to 0.
--- @param version? integer @(optional) The VMEM version to use for upload. Defaults to 1 if `use_peekpoke`, otherwise 2.
--- @param timeout? integer @(optional) Timeout for upload, defaults to global timeout
--- @return Vmem
--- @throw if wrong args or `malloc()` failed
function vmem(node, address, use_peekpoke, version, timeout) end


--- Sleep x ms
--
--- @function sleep
--- @param time_ms integer How long to sleep in ms
function sleep(time_ms) end

--- Gets system time as unix timestamp in ms
--
--- @return integer unix_ms unix time in ms
function get_time() end

--- Ping remote node 
--- uses global timeout variable
--
--- @param node integer Remote node to ping
--- @param size? integer @(optional) The size of the ping packet. Defaults to 0
--- @return integer time_ms echo time in ms, -1 if not response
function ping(node, size) end

--- Ping remote node 
--- uses global timeout variable
--
--- @param node integer Remote node to ping
--- @return integer time_ms echo time in ms, -1 if not response
function ping(node) end

--- Get uptime of remote node
--- uses global timeout variable
--
--- @param node integer Remote node to ping
--- @return integer time_sec uptime in seconds 
function uptime(node) end

--- Reboot node
--
--- @param node integer node to reboot
function reboot(node) end

--- Global timeout variable used for all functions
--- Default is 1000 ms
Timeout = 1000

UINT8 = 0
UINT16 = 1
UINT32 = 2
UINT64 = 3
INT8 = 4
INT16 = 5
INT32 = 6
INT64 = 7
XINT8 = 8
XINT16 = 9
XINT32 = 10
XINT64 = 11
FLOAT = 12
DOUBLE = 13
STRING = 14
DATA = 15

-- Use this global var to pass an argument to a lua script with parameter lua_args
Larg = 0

--- Set status parameter 
--
--- @param state integer State code specific for lua script useful for indicating the internal state the lua script in the lua_state parameter
function set_state(state) end