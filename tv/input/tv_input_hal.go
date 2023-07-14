package  tv_input_hal

import (
    "fmt"
    "android/soong/android"
    "android/soong/cc"
)

func init() {
    android.RegisterModuleType("tv_input_hal_defaults", tv_input_hal_DefaultsFactory)
}

func tv_input_hal_DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, func(ctx android.LoadHookContext) {
        type props struct {
            Cflags []string
        }
        p := &props{}
        p.Cflags = globalDefaults(ctx)
        ctx.AppendProperties(p)
    })

    return module
}

func globalDefaults(ctx android.BaseContext) ([]string) {
    var cppflags []string
    productSupportDtvkit := ctx.AConfig().Getenv("PRODUCT_SUPPORT_DTVKIT")
    if productSupportDtvkit == "true" {
        fmt.Println("-DSUPPORT_DTVKIT")
        cppflags = append(cppflags,"-DSUPPORT_DTVKIT")
    }
    return cppflags
}
