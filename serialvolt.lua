---@diagnostic disable: param-type-mismatch
---@diagnostic disable: need-check-nil
---@diagnostic disable: cast-local-type

local analog_in = analog:channel()
local baud_rate = 9600

if not analog_in:set_pin(15) then 
  gcs:send_text(0, "Invalid analog pin")
end

local port = assert(serial:find_serial(0),"Could not find Scripting Serial Port")


port:begin(baud_rate)
port:set_flow_control(0)

function update() 
  local vav = analog_in:voltage_average()
  if vav > 1 then
  	vehicle:set_mode(4)
	  gcs:send_text(0, "SET HOLD MODE1")
  end

  local n_bytes = port:available()
  while n_bytes > 0 do
    -- only read a max of 515 bytes in a go
    -- this limits memory consumption
    local buffer = {} 
    local bytes_target = n_bytes - math.min(n_bytes, 512)
    while n_bytes > bytes_target do
      table.insert(buffer,port:read())
      n_bytes = n_bytes - 1
    end

      gcs:send_text(0, string.char(table.unpack(buffer)))
    
  end

  return update, 1000 
end

return update, 1000