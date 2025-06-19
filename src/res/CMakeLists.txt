cmrc_add_resource_library(gitkf_res NAMESPACE res
    wwwroot/favicon.svg
    wwwroot/app.css
    wwwroot/app.js
    wwwroot/index.html
    wwwroot/version.js
)
target_sources(gitkf_res PRIVATE res.cpp)