local token = os.getenv("TGW_TOKEN")
if token == nil or token == "" then
    error("TGW_TOKEN is required")
end

wrk.method = "GET"
wrk.headers["Authorization"] = "Bearer " .. token
wrk.headers["X-Trace-Id"] = "wrk-profile-trace"

request = function()
    return wrk.format("GET", "/api/user/profile?user_id=10001")
end