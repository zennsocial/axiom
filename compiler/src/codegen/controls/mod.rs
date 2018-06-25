mod audio_control;
mod audio_extract_control;
mod control_context;
mod graph_control;
mod midi_control;
mod midi_extract_control;
mod roll_control;
mod scope_control;

pub use self::audio_control::AudioControl;
pub use self::audio_extract_control::AudioExtractControl;
pub use self::graph_control::GraphControl;
pub use self::midi_control::MidiControl;
pub use self::midi_extract_control::MidiExtractControl;
pub use self::roll_control::RollControl;
pub use self::scope_control::ScopeControl;

use self::control_context::{ControlContext, ControlUiContext};
use ast::{ControlField, ControlType};
use codegen::{
    build_context_function, util, values, BuilderContext, LifecycleFunc, TargetProperties,
};
use inkwell::builder::Builder;
use inkwell::context::Context;
use inkwell::module::{Linkage, Module};
use inkwell::types::StructType;
use inkwell::values::{FunctionValue, PointerValue};
use inkwell::AddressSpace;
use mir::VarType;
use std::fmt;

pub type ControlFieldGeneratorCb = Fn(&mut ControlContext, PointerValue);

pub trait ControlFieldGenerator {
    fn generate(
        &self,
        field: ControlField,
        get_cb: &ControlFieldGeneratorCb,
        set_cb: &ControlFieldGeneratorCb,
    );
}

struct PrivateGenerator<'a> {
    module: &'a Module,
}

impl<'a> ControlFieldGenerator for PrivateGenerator<'a> {
    fn generate(
        &self,
        field: ControlField,
        get_cb: &ControlFieldGeneratorCb,
        set_cb: &ControlFieldGeneratorCb,
    ) {
        build_field_func(
            self.module,
            get_field_getter_func(self.module, field),
            get_cb,
        );
        build_field_func(
            self.module,
            get_field_setter_func(self.module, field),
            set_cb,
        );
    }
}

pub fn get_group_type(context: &Context, control_type: ControlType) -> StructType {
    values::remap_type(context, &VarType::of_control_value(&control_type))
}

pub fn get_field_type(context: &Context, field: ControlField) -> StructType {
    values::remap_type(context, &VarType::of_control_field(&field))
}

pub fn get_data_type(context: &Context, control_type: ControlType) -> StructType {
    match control_type {
        ControlType::Audio => AudioControl::data_type(context),
        ControlType::Midi => MidiControl::data_type(context),
        ControlType::AudioExtract => AudioExtractControl::data_type(context),
        ControlType::MidiExtract => MidiExtractControl::data_type(context),
        ControlType::Graph => GraphControl::data_type(context),
        ControlType::Roll => RollControl::data_type(context),
        ControlType::Scope => ScopeControl::data_type(context),
    }
}

pub fn get_ui_type(context: &Context, control_type: ControlType) -> StructType {
    match control_type {
        ControlType::Audio => AudioControl::ui_type(context),
        ControlType::Midi => MidiControl::ui_type(context),
        ControlType::AudioExtract => AudioExtractControl::ui_type(context),
        ControlType::MidiExtract => MidiExtractControl::ui_type(context),
        ControlType::Graph => GraphControl::data_type(context),
        ControlType::Roll => RollControl::data_type(context),
        ControlType::Scope => ScopeControl::data_type(context),
    }
}

fn get_lifecycle_func(
    module: &Module,
    control_type: ControlType,
    lifecycle: LifecycleFunc,
) -> FunctionValue {
    let func_name = format!("maxim.control.{}.{}", control_type, lifecycle);
    util::get_or_create_func(module, &func_name, &|| {
        let context = module.get_context();
        (
            Linkage::ExternalLinkage,
            context.void_type().fn_type(
                &[
                    &get_group_type(&context, control_type).ptr_type(AddressSpace::Generic),
                    &get_data_type(&context, control_type).ptr_type(AddressSpace::Generic),
                ],
                false,
            ),
        )
    })
}

fn get_ui_lifecycle_func(
    module: &Module,
    control_type: ControlType,
    lifecycle: LifecycleFunc,
) -> FunctionValue {
    let func_name = format!("maxim.control.{}.ui_{}", control_type, lifecycle);
    util::get_or_create_func(module, &func_name, &|| {
        let context = module.get_context();
        (
            Linkage::ExternalLinkage,
            context.void_type().fn_type(
                &[
                    &get_group_type(&context, control_type).ptr_type(AddressSpace::Generic),
                    &get_data_type(&context, control_type).ptr_type(AddressSpace::Generic),
                    &get_ui_type(&context, control_type).ptr_type(AddressSpace::Generic),
                ],
                false,
            ),
        )
    })
}

fn get_field_getter_setter_func(
    module: &Module,
    field: ControlField,
    func_name: &str,
) -> FunctionValue {
    util::get_or_create_func(module, &func_name, &|| {
        let control_type = ControlType::from(field);
        let context = module.get_context();
        (
            Linkage::ExternalLinkage,
            context.void_type().fn_type(
                &[
                    &get_group_type(&context, control_type).ptr_type(AddressSpace::Generic),
                    &get_data_type(&context, control_type).ptr_type(AddressSpace::Generic),
                    &get_field_type(&context, field).ptr_type(AddressSpace::Generic),
                ],
                false,
            ),
        )
    })
}

pub fn get_field_getter_func(module: &Module, field: ControlField) -> FunctionValue {
    get_field_getter_setter_func(module, field, &format!("maxim.control.{}.getter", field))
}

pub fn get_field_setter_func(module: &Module, field: ControlField) -> FunctionValue {
    get_field_getter_setter_func(module, field, &format!("maxim.control.{}.setter", field))
}

pub fn build_field_get(
    module: &Module,
    builder: &mut Builder,
    field: ControlField,
    group_ptr: PointerValue,
    data_ptr: PointerValue,
    out_val: PointerValue,
) {
    let func = get_field_getter_func(module, field);
    builder.build_call(
        &func,
        &[&group_ptr, &data_ptr, &out_val],
        "field.get",
        false,
    );
}

pub fn build_field_set(
    module: &Module,
    builder: &mut Builder,
    field: ControlField,
    group_ptr: PointerValue,
    data_ptr: PointerValue,
    in_val: PointerValue,
) {
    let func = get_field_setter_func(module, field);
    builder.build_call(&func, &[&group_ptr, &data_ptr, &in_val], "field.set", false);
}

pub fn build_lifecycle_call(
    module: &Module,
    builder: &mut Builder,
    control_type: ControlType,
    lifecycle: LifecycleFunc,
    group_ptr: PointerValue,
    data_ptr: PointerValue,
) {
    let func = get_lifecycle_func(module, control_type, lifecycle);
    builder.build_call(&func, &[&group_ptr, &data_ptr], "", false);
}

pub fn build_ui_lifecycle_call(
    module: &Module,
    builder: &mut Builder,
    control_type: ControlType,
    lifecycle: LifecycleFunc,
    group_ptr: PointerValue,
    data_ptr: PointerValue,
    ui_ptr: PointerValue,
) {
    let func = get_ui_lifecycle_func(module, control_type, lifecycle);
    builder.build_call(&func, &[&group_ptr, &data_ptr, &ui_ptr], "", false);
}

fn build_lifecycle_func(
    module: &Module,
    control: ControlType,
    lifecycle: LifecycleFunc,
    builder: &Fn(&mut ControlContext),
) {
    let func = get_lifecycle_func(module, control, lifecycle);
    build_context_function(module, func, &|ctx: BuilderContext| {
        let val_ptr = ctx.func.get_nth_param(0).unwrap().into_pointer_value();
        let data_ptr = ctx.func.get_nth_param(1).unwrap().into_pointer_value();
        let mut control_context = ControlContext {
            ctx,
            val_ptr,
            data_ptr,
        };
        builder(&mut control_context);

        control_context.ctx.b.build_return(None);
    });
}

fn build_ui_lifecycle_func(
    module: &Module,
    control: ControlType,
    lifecycle: LifecycleFunc,
    builder: &Fn(&mut ControlUiContext),
) {
    let func = get_ui_lifecycle_func(module, control, lifecycle);
    build_context_function(module, func, &|ctx: BuilderContext| {
        let val_ptr = ctx.func.get_nth_param(0).unwrap().into_pointer_value();
        let data_ptr = ctx.func.get_nth_param(1).unwrap().into_pointer_value();
        let ui_ptr = ctx.func.get_nth_param(2).unwrap().into_pointer_value();
        let mut control_context = ControlUiContext {
            ctx,
            val_ptr,
            data_ptr,
            ui_ptr,
        };
        builder(&mut control_context);

        control_context.ctx.b.build_return(None);
    });
}

fn build_field_func(module: &Module, func: FunctionValue, builder: &ControlFieldGeneratorCb) {
    build_context_function(module, func, &|ctx: BuilderContext| {
        let val_ptr = ctx.func.get_nth_param(0).unwrap().into_pointer_value();
        let data_ptr = ctx.func.get_nth_param(1).unwrap().into_pointer_value();
        let field_ptr = ctx.func.get_nth_param(2).unwrap().into_pointer_value();
        let mut control_context = ControlContext {
            ctx,
            val_ptr,
            data_ptr,
        };
        builder(&mut control_context, field_ptr);

        control_context.ctx.b.build_return(None);
    });
}

pub trait Control {
    fn control_type() -> ControlType;

    fn data_type(context: &Context) -> StructType {
        context.struct_type(&[], false)
    }

    fn ui_type(context: &Context) -> StructType {
        context.struct_type(&[], false)
    }

    fn gen_construct(_control: &mut ControlContext) {}

    fn gen_update(_control: &mut ControlContext) {}

    fn gen_destruct(_control: &mut ControlContext) {}

    fn gen_ui_construct(_control: &mut ControlUiContext) {}

    fn gen_ui_update(_control: &mut ControlUiContext) {}

    fn gen_ui_destruct(_control: &mut ControlUiContext) {}

    fn gen_fields(generator: &ControlFieldGenerator);

    fn build_lifecycle_funcs(module: &Module) {
        build_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Construct,
            &Self::gen_construct,
        );
        build_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Update,
            &Self::gen_update,
        );
        build_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Destruct,
            &Self::gen_destruct,
        );
    }

    fn build_ui_lifecycle_funcs(module: &Module) {
        build_ui_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Construct,
            &Self::gen_ui_construct,
        );
        build_ui_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Update,
            &Self::gen_ui_update,
        );
        build_ui_lifecycle_func(
            module,
            Self::control_type(),
            LifecycleFunc::Destruct,
            &Self::gen_ui_destruct,
        );
    }

    fn build_field_funcs(module: &Module) {
        Self::gen_fields(&PrivateGenerator { module });
    }

    fn build_funcs(module: &Module, target: &TargetProperties) {
        Self::build_lifecycle_funcs(module);
        Self::build_field_funcs(module);

        if target.include_ui {
            Self::build_ui_lifecycle_funcs(module);
        }
    }
}

pub fn default_copy_getter(control: &mut ControlContext, out_val: PointerValue) {
    util::copy_ptr(control.ctx.b, control.ctx.module, control.val_ptr, out_val);
}

pub fn default_copy_setter(control: &mut ControlContext, in_val: PointerValue) {
    util::copy_ptr(control.ctx.b, control.ctx.module, in_val, control.val_ptr);
}
