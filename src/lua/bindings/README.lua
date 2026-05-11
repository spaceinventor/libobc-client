-- READ THIS EXAMPLE DOCUMENT!
-- Before writing a lua script using SI custom LUA implementation.
-- First part is a guide to using the custom modules.
-- They give access to libcsp and libparam funnctions.
-- There is also a small section with a quick run through of LUA

----------------------------------------------------
-- 1. Custom lua modules
----------------------------------------------------
---NOTICE:
-- You cannot load external libraries with require()
-- You do not have access to io, os utf8 or debug functions
-- You cannot use str_dump()
--
--
-- Put the luasi.lua file next to your lua script
-- Makes IDE code completion work with custom modules.
--
-- Parameters created by lua will only exist
-- within the lua state
-- and be removed when the lua script ends.
--
-- Most function calls can throw errors on
-- timeouts, csp errors, wrong arguments passed

-- set global timeout for all functions to 1500 ms default is 1000 ms
Timeout = 1500

-- Use this global var to pass an argument to a lua script with parameter lua_args
if Larg == 42 then
	print("Larg is 42")
	-- signal in lua_state parameter
	set_state(42)
end

-- Create remote parameter with id 120 and node 145
-- of type uint8_t with array size 8 could also use "u08" for type
local ch_on_remote = parameter(120, 145, UINT8, 8)

local boot_img0 = parameter(21, 13, UINT8)
local boot_img1 = parameter(20, 13, UINT8)
local boot_img2 = parameter(22, 13, UINT8)
local boot_img3 = parameter(23, 13, UINT8)

-- Sets a remote parameter to 1
-- NOTICE we use 0 indice on params
ch_on_remote:set(0, 1)
-- Without index also works defaults to 0
ch_on_remote:set(1)
-- Gets a remote paramter
-- every call to a :get() or :set()
-- will access the remote node value!
local ch_on0_val = ch_on_remote:get(0)
-- Without index returns whole array as table
local ch_on0_val_tbl = ch_on_remote:get()

-- Here we want to loop over a parameter
-- but we don't want to get the remote parameter
-- twice when used inside the loop
local cur_val
while true do
	cur_val = ch_on_remote:get(0)
	if cur_val == 0 then
		break
	end
	print(cur_val) -- print to stdbuf
	sleep(1000) -- sleep 1000 ms
end

-- print the length of the array parameter
print(#ch_on_remote)
-- You can read properties of the parameter
print(ch_on_remote.node)
print(ch_on_remote.id)
print(ch_on_remote.type) -- This is the type as integer
print(ch_on_remote.length)

print(get_time()) -- get system time as unix timestamp in ms
print(uptime(10)) -- print uptime of remote node
ping(10) -- ping remote node 10
reboot(10) -- reboot node 10

-- Create parameter list containing boot_img0 and boot_img1
-- This list can push your modified cache parameter values to the remote node
-- Only changes in the parameter will be pushed
-- Pulling the parameters will reset its modified state to false
-- Can also pull the remote values to the cached parameter values
plist_ch_on = parameter_list(ch_on_remote)
plist_bootimgs_23 = parameter_list()
-- you can also modify the list after created
plist_bootimgs_23:add({ boot_img2, boot_img3 })
plist_bootimgs_23:remove(boot_img2)

-- Set local value of single parameter
boot_img0.cache = 42

-- Set local value of array parameter
ch_on_remote.cache[0] = 1

-- Pushing the parameter list will automatically
-- Set the changed flag of index 0
plist_ch_on:push()
-- Pusing again will not push anything
-- as modified flag of all the values of the parameters
-- in the parameter list is false
plist_ch_on:push()

-- setting cached value then pulling remotes values
-- will reset the modified status
-- of the parameters in the parameter list
ch_on_remote.cache[0] = 1
plist_ch_on:pull()
-- this push does nothing
plist_ch_on:push()

-- Example of how to protect against thrown errors and retry logic
local maxRetries = 3
local retries = 0
local status, result

repeat
	status, result = pcall(function()
		-- If an error occurs here like timeouts or CSP errors,
		-- it will be caught by pcall
		return ch_on_remote:get(1)
	end)

	if not status then
		-- An error occurred, result contains the error message
		print("Attempt failed:", result)
		retries = retries + 1
	end
until status or retries >= maxRetries

if status then
	-- No error, result contains the function's return value
	print("Success:", result)
else
	-- All attempts failed, handle as needed
	print("Error after " .. maxRetries .. " retries:", result)
end
