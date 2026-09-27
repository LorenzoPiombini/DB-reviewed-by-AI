db.order_transaction=function(head,lines,fn) return fn() end
-- Run against the real db_config.lua with storage functions substituted.
local writes, keys, fail_line, fail_head, fail_key
get_numeric_key = function() keys=keys+1; if fail_key then return nil end; return 101 end
g_offset = function() return 0 end
write_record = function(file, record, key)
    writes=writes+1
    if file==sales_orders.lines and fail_line then return nil,"storage error" end
    if file==sales_orders.head and fail_head then return nil,"storage error" end
    return key,record
end
local function order()
    return {fields={sales_orders_head={lines_nr=2},sales_orders_lines={
        {fields={qty=1}},{fields={qty=2}}
    }}}
end
local function reset()
    writes,keys=0,0
    fail_line,fail_head,fail_key=false,false,false
end
local function invalid(data)
    reset()
    local key,err=write_orders(data)
    assert(key==nil and err==VALUE_ERROR)
    assert(writes==0 and keys==0, 'invalid order must have no writes or key allocation')
end
invalid(nil)
invalid({})
for _,value in ipairs({0,-1,'2',math.huge,0/0}) do
    local data=order(); data.fields.sales_orders_lines[2].fields.qty=value; invalid(data)
end
for _,value in ipairs({0,-1,1.5,'2',3,math.huge,0/0}) do
    local data=order(); data.fields.sales_orders_head.lines_nr=value; invalid(data)
end
local data=order(); data.fields.sales_orders_lines[2]={}; invalid(data)
reset(); local key,err=write_orders(order()); assert(key==101 and err==0 and writes==3)
reset(); fail_line=true; key,err=write_orders(order()); assert(key==nil and err==-22 and writes==1)
reset(); fail_head=true; key,err=write_orders(order()); assert(key==nil and err==-21 and writes==3)
reset(); fail_key=true; key,err=write_orders(order()); assert(key==nil and err==-23 and writes==0)
print('PASS: real Lua order validation before writes, success, line/head/key errors')
