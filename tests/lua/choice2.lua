local a = Local("a", Bool)
local b = Local("b", Bool)
local c = mk_choice(a, b)
print(c)
local env = environment()
check_error(function() env:infer_type(c) end)
