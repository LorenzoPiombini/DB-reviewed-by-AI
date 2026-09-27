db = require("db")

TEST = true
--- DB GLOBALS
ORDER_BASE = 100
KEY_NOT_FOUND = 16
INDEX_OUT_OF_RANGE = 18 
CACHE_FAILED = 19

--- database files
-- name_file = "db/name_file" /* i do not need it for now */
customers = "/root/db/customer"
price_level = "/root/db/price_level"
items = "/root/db/item"
sales_orders = {}
sales_orders["head"] = "/root/db/sales_orders_head"
sales_orders["lines"] = "/root/db/sales_orders_lines"

--- database crud functions
write_record = db.write_record
update_record = db.update_record
get_numeric_key = db.get_numeric_key
string_data_to_add_template = db.string_data_to_add_template
indexing = db.save_key_at_index
create_rec = db.create_record
g_rec = db.get_record
d_rec = db.delete_record
g_all_key = db.get_all_key
g_offset = db.get_offset

--- look for documentation in lua/src/export_db_lua.c
--- return two results, the record created and its key, if the key is not passed
--- as an argument the key will be computed automatically
function w_rec(file_name, data, key, number)
	if key == nil then
		return write_record(file_name, data)
	elseif key == "base" and number ~= nil then
		return write_record(file_name, data, key, number)
	elseif key ~= "base" then
		return write_record(file_name, data, key)
	end
end

--- return the key of the name_file record
function write_to_name_file(data)
	local key, rec = write_record(name_file, data)
	if key == nil or rec == nil then
		return -1
	end
	return key
end

function write_customers(data)

	local f = data.fields

	-- indexing function
	-- we are saving the same record with a different key, to get better
	-- searching performance,data integrety  and user experience
	-- if this one fails, it means that we have this customer in the DB already
	-- USING INDEX 2 BECAUSE INDEX 1 and 0 ARE USED INTERNALY FROM THE DB system
	-- @@ we are creating a reference to the customer record, based on the customer name @@
	data.offset = g_offset(customers);
	if data.offset == nil then return -1 end

	local res = indexing(customers, f.name, 2, data.offset)
	if res ~= 2 then
		return -1
	end

	local k, rec = w_rec(customers, data)
	if k >= 0 then
		return 0,k
	else
		return -1
	end
end

function update_orders(orders_head, orders_lines, key)
	local head_rec = create_rec(sales_orders.head,orders_head)
	if head_rec == nil then
		return nil
	end

	local f = head_rec.fields
	if f == nil then
		return nil
	end

	-- update the lines first
	if f.lines_nr == 1 then
		local k_line = string.format("%s/%d", tostring(key), f.lines_nr)
		local line = string.sub(orders_lines, 2, -2)
		local data = string.sub(line, 3, #line)
		local up = update_record(sales_orders.lines, data, k_line)
		if up == nil then
			return nil
		end
	else
		for i = 1, f.lines_nr do
			local k_line = string.format("%s/%d", tostring(key), i)
			local line
			local ending, sub_str_ending = string.find(orders_lines, "],")
			if ending == nil then
				line = string.sub(orders_lines, 2, -2)
				line = string.sub(line, 3, #line)
			else
				line = string.sub(orders_lines, 1, ending)
				orders_lines = string.sub(orders_lines, sub_str_ending + 1, #orders_lines)
				line = string.sub(line, 2, -2)
				line = string.sub(line, 3, #line)
			end
			local up = update_record(sales_orders.lines, line, k_line)
			if up == KEY_NOT_FOUND  then
				local r = write_record(sales_orders.lines,line,k_line)
				if r == nil then 
					print("cannot write!") 
					return -1 
				end
			elseif up ~= 0 then
				print("error!")
				return -1
			end
		end
	end
	
	-- check if lines are different, 
	local head_on_file,err = g_rec(sales_orders.head,key)
	if head_on_file == nil then 
		print(err)
		return -1 
	end

	if head_on_file.fields.lines_nr > f.lines_nr then
		-- we need to delete some records in sales_orders_lines
		for i = f.lines_nr + 1, head_on_file.fields.lines_nr do
			local k = string.format("%d/%d",key,i)
			--delete record with key k
			local r,message = d_rec(sales_orders.lines,k)
			if r ~= 0 then 
				print("error in cleaning sales_order_lines");
				print(message)
				return -1
			end
		end
	end

	-- if update line succseed update head
	local up = update_record(sales_orders.head, orders_head, key)
	if up == nil then
		return nil
	end

	return 0
end

function write_orders(orders_head, orders_lines)
	local kh, ord_head = w_rec(sales_orders.head, orders_head, "base", ORDER_BASE)

	if ord_head == nil or kh == nil then
		return nil
	end

	local f = ord_head.fields
	if f.lines_nr == 1 then
		local key_line = string.format("%d/%d", kh, f.lines_nr)
		-- the lines of the orders will be a string like this:
		-- [w|item:Soccer shoes:uom:pair:qty:43:disc:2.2:unit_price:200:total:8410.80:request_date:1-8-26]
		-- string.sub(orders_lines,2,-2) return a string without []
		-- and the following string sub return the string without w|
		local line = string.sub(orders_lines, 2, -2)
		local data = string.sub(line, 3, #line)
		local kl, ord_lines = w_rec(sales_orders.lines, data, key_line)
	else
		for i = 1, f.lines_nr do
			local key_line = string.format("%d/%d", kh, i)
			local line
			local ending, sub_str_ending = string.find(orders_lines, "],")
			if ending == nil then
				line = string.sub(orders_lines, 2, -2)
				line = string.sub(line, 3, #line)
			else
				line = string.sub(orders_lines, 1, ending)
				orders_lines = string.sub(orders_lines, sub_str_ending + 1, #orders_lines)
				line = string.sub(line, 2, -2)
				line = string.sub(line, 3, #line)
			end
			local kh, ord_lines = w_rec(sales_orders.lines, line, key_line)
			if ord_lines == nil then
				return nil
			end
		end
	end

	return kh
end

local function rec_to_json(rec)
	local f = rec.fields
	local json = "{ "
	for key, value in pairs(f) do
		local sb
		if type(value) == "string" then
			sb = string.format('"%s":"%s",', key, value)
		elseif type(value) == "number" then
			if math.type(value) == "integer" then
				sb = string.format('"%s":"%d",', key, value)
			else
				sb = string.format('"%s":"%.2f",', key, value)
			end
		end
		json = string.format("%s%s", json, sb)
	end
	json = string.sub(json, 1, #json - 1)
	json = string.format("%s%s", json, "}")
	return json
end

function get_item(key)
	local item,err
	if type(key) == 'string' then
		item,err = g_rec(items,key,1)
	else
		item,err = g_rec(items,key)
	end

	if item == nil then 
		print(err)
		return nil 
	end
	
	return rec_to_json(item)
end

function get_customer(key)
	local cust,pr_l,err
	if type(key) == 'string' then
		local sanitized_key = string.gsub(key,"%%%d+"," ") -- decode URL encoding
		cust,err = g_rec(customers,sanitized_key,2); -- 2 is the index number in the file	
	else
		cust,err = g_rec(customers,key)
	end

	if cust == nil  then
		print(err)
		return nil
	end


	if cust["price_level_id"] ~= nil then
		pr_l,err = g_rec(price_level,cust.price_level_id,1) -- 1 is the file index
		if pr_l == nil then
			print(err)
			return nil
		end
	end
	
	return rec_to_json(cust)
end

function get_customer_for_new_sales_order(key)
	local cust,pr_l,err
	cust,err = g_rec(customers,key,2) -- 2 is the index number in the file	

	if cust == nil  then 
		print(err)
		return nil 
	end

	if cust.fields["price_level_id"] ~= nil then
		-- get price_level data
		pr_l = g_rec(price_level,cust.fields.price_level_id,1) -- 1 is the file index
		if pr_l == nil then
			print(err)
			return nil
		end
		-- create a json string that will be used to populate the new_sales_order
		local json_price_level = string.gsub(rec_to_json(pr_l), "{", ",")
		local json_cust =  string.gsub(rec_to_json(cust),"}","")
		return string.format("%s%s",json_cust,json_price_level)
	end
	

	return rec_to_json(cust)
end

-- the get_order function has to rebuild the order!
-- the order data are spread between 
-- 	@ sales_order_head
-- 	@ sales_order_lines
-- 	@ items
-- 	@ price_level
-- this is because we want to maintain the database normalized
function get_order(data)
	local ord,pr_l,item,err
	ord,err = g_rec(sales_orders.head, data)

	if ord == nil then
		print(err)
		return nil 
	end

	local disc = 0
	if ord.fields.price_level_id ~= nil then
		pr_l,err = g_rec(price_level,ord.fields.price_level_id,1)
		if pr_l == nil then
			print(err)
			return nil
		end
		disc = pr_l.fields.percentage
	end

	local head_json = rec_to_json(ord)

	local json = string.format('{ "sales_orders_head":%s,"sales_orders_lines":{', head_json)
	for i = 1, ord.fields.lines_nr do
		local key_line = string.format("%d/%d", data, i)
		local line,err = g_rec(sales_orders.lines, key_line)

		if line == nil then 
			print(err)
			return nil
		end

		-- change the date format to conform with what the browser expect  
		if line.fields.request_date ~= nil then
			m,d,y = string.match(line.fields.request_date,"(%d+)-(%d+)-(%d+)")
			y = tonumber(y) + 2000
			line.fields.request_date = string.format('%s-%s-%s',y,m,d)
		end

		item, err= g_rec(items,line.fields.item_id,1)
		if item == nil then
			print(err)
			return nil
		end

		local total = item.fields.unit_price * line.fields.qty
		if disc ~= 0 then
			total = total * ((100 - disc)/100)
		end
		local str_to_add_to_line = string.format('"uom":"%s","unit_price":"%.2f","disc":"%.2f","total":"%.2f"',
								item.fields.uom,
								item.fields.unit_price,
								disc,
								total)

		local line_title = string.format("line_%d", i)
		local line_from_file = rec_to_json(line)
		line_from_file = string.sub(line_from_file,2,-1)
		json = string.format('%s"%s":{%s,%s,', json, line_title,str_to_add_to_line,line_from_file)
	end

	json = string.sub(json,1,#json-1)
	json = string.format("%s}}", json)
	return json
end

-- helper function for reports on open orders
local function get_orders_total(keys_head)
	local totals = {}
	local orders_tot = 0.0
	for n in string.gmatch(keys_head,"%d+") do
		local k = tonumber(n)
		local h,err = g_rec(sales_orders.head,k)

		if h == nil then
			print(err)
			return -1 
		end

		local disc = 0
		if h.fields.price_level_id ~= nil then
			local pr_l,err = g_rec('/root/db/price_level',h.fields.price_level_id,1)
			if pr_l == nil then
				print(err)
				return -2 
			end
			disc = pr_l.fields.percentage
		end

		local total = 0
		for i = 1, h.fields.lines_nr do
			local k_lines = string.format("%d/%d",k,i)
			local line,err_l = g_rec(sales_orders.lines,k_lines)
			if line == nil then 
				print(err_l)
				return -3 
			end

			local item,err_i = g_rec('/root/db/item',line.fields.item_id,1)
			if item == nil then print(err_i) return -4 end

			total = total + item.fields.unit_price * line.fields.qty
			if disc ~= 0 then
				total = total * ((100-disc)/ 100)
			end
		end
		orders_tot = orders_tot + total
		totals[n] = string.format('"%s":"%s","%s":%.2f',"customer_id",h.fields.customer_id,"total",total)
	end
	totals["orders_total"] = string.format('%.2f',orders_tot)
	return totals
end

-- function signature 
-- this will be use from work_process.c to execute the function open_orders()
-- this way we won't need to recompile the C back end all the time 
-- when adding a new report

open_orders_s = ">s"
sales_orders_week_s = ">s"
overdue_orders_s = ">s"
-----------------------------------------------------------------------

function open_orders()
	local all_head_k = g_all_key(sales_orders.head,0)
	if type(all_head_k) ~= "string" then return -1 end

	local totals = get_orders_total(all_head_k)
	if totals == -1 then 
		print("cannot get sales_order_head data!")
		return '{message: "server error"}'
	elseif totals == -2 then 
		print("cannot get price_level data!")
		return '{message: "server error"}'
	elseif totals == -3 then 
		print("cannot get sales_order_line data!")
		return '{message: "server error"}'
	elseif totals == -4 then 
		print("cannot get item data!")
		return '{message: "server error"}'
	end

	local json = "{"
	for n in string.gmatch(all_head_k,"%d+") do
		json = string.format('%s"%d":{%s},',json,n,totals[n])
	end
	json = string.format('%s"orders_total":%.2f}',json,totals['orders_total'])
	return json
end


-- return begining of the current week in seconds (since january 1 1970) 
local function get_week_start()
	today_second = os.time()
	start = os.date('%w',today_second)
	if start == 0 then
		return today_second
	end
	return today_second - (start *(60*60*24))
end

-- return the end of the current week in seconds (since january 1 1970) 
local function get_week_end()
	today_second = os.time()
	finish = os.date('%w',today_second)
	if finish == 6 then
		return today_second
	end
	return today_second + ((finish +(6 - finish)) *(60*60*24))
end

local function convert_date(date)
	if date == nil then return {year  = 1987 ,month = 3, day = 2} end
	m,d,y = string.match(date,"(%d+)-(%d+)-(%d+)")
	y = tonumber(y) + 2000
	date = {year  = y ,month = m, day = d}
	return os.time(date)
end

local function is_date_this_week(date_second)
	start = get_week_start()
	finish = get_week_end()
	return (start <= date_second) and (finish >= date_second)
end

local function get_orders_by_week_range(head_keys)
	local result = '{"message":['
	local exit = false
	for k,v in pairs(head_keys) do
		local h,err_h = g_rec(sales_orders.head,v)

		if h == nil then print(err_h) return -1 end

		for i = 1, h.fields.lines_nr do
			local k_lines = string.format("%d/%d",v,i)
			local line,err_l = g_rec(sales_orders.lines,k_lines)
			if line == nil then print(err_l) return -1 end
			
			if is_date_this_week(convert_date(line.fields.request_date)) == true then
				if string.sub(result,-1) == '[' then
					result = string.format('%s%d',result,v)
				else
					result = string.format('%s,%d',result,v)
				end
			elseif convert_date(line.fields.request_date) > get_week_end() then
				exit = true
				break
			end
		end
		if exit then
			break
		end
	end
	
	if string.sub(result,-1) == '[' then
		return '{"message":[]}'
	end

	if string.sub(result,-1) == ',' then
		result = string.sub(result, 1, #result - 1)
		return string.format('%s]}',result)
	else
		return string.format('%s]}',result)
	end
	
end

function sales_orders_week()
	local all_head_k = g_all_key(sales_orders.head,0)
	if type(all_head_k) ~= "string" then return -1 end

	-- we sort the keys, to increase performance (NOTE: FOR NOW!)
	-- we need to come up with better solutions
	-- this patch run on the assumption that the sales order file contains only the open orders
	-- which will be the case in real life, but the open orders could be thousands
	local ordered_keys = {}
	for n in string.gmatch(all_head_k,"%d+") do
		table.insert(ordered_keys,tonumber(n))
	end
	table.sort(ordered_keys)

	return get_orders_by_week_range(ordered_keys)
end

local function is_date_in_the_past(date)
	one_day_seconds = 60*60*24
	now = os.time()
	if now - date < one_day_seconds then
		-- TODO: convert to table and check the date
	end
	return (now - date) > one_day_seconds
end

local function get_overdue_order(head_keys)
	local result = '{"message":['
	local exit = false
	now = os.time()
	for k,v in pairs(head_keys) do
		local h,err_h = g_rec(sales_orders.head,v)

		if h == nil then print(err_h) return -1 end

		for i = 1, h.fields.lines_nr do
			local k_lines = string.format("%d/%d",v,i)
			local line,err_l = g_rec(sales_orders.lines,k_lines)
			if line == nil then print(err_l) return -1 end
			
			if is_date_in_the_past(convert_date(line.fields.request_date)) == true then
				if i == 1 then
					if string.sub(result,-1) == '[' then
						result = string.format('%s"%s"',result,v)
					else
						result = string.format('%s,"%s"',result,v)
					end
				end
			end
		end
	end
	
	if string.sub(result,-1) == '[' then
		return '{"message":[]}'
	end

	if string.sub(result,-1) == ',' then
		result = string.sub(result, 1, #result - 1)
		return string.format('%s]}',result)
	else
		return string.format('%s]}',result)
	end
	
end

-- helper function for reports on open orders
local function get_orders_total(keys_head)
	local totals = {}
	local orders_tot = 0.0
	for n in string.gmatch(keys_head,"%d+") do
		local k = tonumber(n)
		local h,err = g_rec(sales_orders.head,k)

		if h == nil then
			print(err)
			return -1 
		end

		local disc = 0
		if h.fields.price_level_id ~= nil then
			local pr_l,err = g_rec('/root/db/price_level',h.fields.price_level_id,1)
			if pr_l == nil then
				print(err)
				return -2 
			end
			disc = pr_l.fields.percentage
		end

		local total = 0
		for i = 1, h.fields.lines_nr do
			local k_lines = string.format("%d/%d",k,i)
			local line,err_l = g_rec(sales_orders.lines,k_lines)
			if line == nil then 
				print(err_l)
				return -3 
			end

			local item,err_i = g_rec('/root/db/item',line.fields.item_id,1)
			if item == nil then print(err_i) return -4 end

			total = total + item.fields.unit_price * line.fields.qty
			if disc ~= 0 then
				total = total * ((100-disc)/ 100)
			end
		end
		orders_tot = orders_tot + total
		totals[n] = string.format('"%s":"%s","%s":%.2f',"customer_id",h.fields.customer_id,"total",total)
	end
	totals["orders_total"] = string.format('%.2f',orders_tot)
	return totals
end

function overdue_orders()
	local all_head_k = g_all_key(sales_orders.head,0)
	if type(all_head_k) ~= "string" then return -1 end

	local ordered_keys = {}
	for n in string.gmatch(all_head_k,"%d+") do
		table.insert(ordered_keys,tonumber(n))
	end
	table.sort(ordered_keys)
	local r = get_overdue_order(ordered_keys);
	local k = string.match(r,"%[.-%]")

	local totals = get_orders_total(k)

	local json = "{"
	for n in string.gmatch(k,"%d+") do
		json = string.format('%s"%d":{%s},',json,n,totals[n])
	end

	json = string.format('%s"orders_total":%.2f}',json,totals['orders_total'])
	return json
end
