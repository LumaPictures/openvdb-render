import pymel.core as pm
import maya.cmds
import re, os

from channelController import channelController


class CacheLimit:
    def __init__(self):
        self.limit = maya.cmds.vdb_visualizer_volume_cache(query=True, limit=True)

    def get(self):
        return self.limit

    def set(self, new_limit):
        self.limit = new_limit


class AEvdb_visualizerTemplate(pm.uitypes.AETemplate, channelController):
    def delete_ui(self, ui):
        try:
            pm.deleteUI(ui)
        except:
            pass

    def clear_popups(self, grp):
        popups = pm.textFieldGrp(grp, query=True, popupMenuArray=True)
        if popups is not None and type(popups) is list and len(popups) > 0:
            for popup in popups:
                self.delete_ui(popup)

    @staticmethod
    def setup_popup_menu_elems(popup, group, param_name):
        pm.popupMenu(popup, edit=True, deleteAllItems=True)
        grid_names_str = pm.getAttr("%s.gridNames" % param_name.split(".")[0])
        if grid_names_str is not None and len(grid_names_str) > 0:
            for each in grid_names_str.split(" "):
                pm.menuItem(label=each, parent=popup, command='pm.setAttr("%s", "%s", type="string"); pm.textFieldGrp("%s", edit=True, text="%s")' % (param_name, each, group, each))

    def create_channel(self, annotation, channel_name, param_name):
        grp = "OpenVDB%sChannelGrp" % channel_name
        pm.textFieldGrp(grp, label=annotation, useTemplate="attributeEditorPresetsTemplate")
        self.update_channel(channel_name, param_name)

    def update_channel(self, channel_name, param_name):
        grp = "OpenVDB%sChannelGrp" % channel_name
        attr_value = pm.getAttr(param_name)
        pm.textFieldGrp(grp, edit=True,
                        text="" if attr_value is None else attr_value,
                        changeCommand=lambda val: pm.setAttr(param_name, val))
        pm.scriptJob(parent=grp,
                     replacePrevious=True,
                     attributeChange=[param_name,
                                      lambda : pm.textFieldGrp(grp, edit=True,
                                                              text=pm.getAttr(param_name))])
        self.clear_popups(grp)
        pm.popupMenu(parent=grp, postMenuCommand=lambda popup, popup_parent: AEvdb_visualizerTemplate.setup_popup_menu_elems(popup, popup_parent, param_name))

    def add_channel_control(self, annotation, channel_name, param_name):
        def create_func(param_name):
            self.create_channel(annotation, channel_name, param_name)
        def update_func(param_name):
            self.update_channel(channel_name, param_name)
        self.callCustom(create_func, update_func, param_name)

    # TODO : maybe use something like templates in c++?
    def create_scattering_channel(self, param_name):
        self.create_channel("Scattering Channel", "scattering", param_name)

    def update_scattering_channel(self, param_name):
        self.update_channel("scattering", param_name)

    def create_attenuation_channel(self, param_name):
        self.create_channel("Attenuation Channel", "attenuation", param_name)

    def update_attenuation_channel(self, param_name):
        self.update_channel("attenuation", param_name)

    def create_emission_channel(self, param_name):
        self.create_channel("Emission Channel", "emission", param_name)

    def update_emission_channel(self, param_name):
        self.update_channel("emission", param_name)

    def create_smoke_channel(self, param_name):
            self.create_channel("Color Channel", "smoke", param_name)

    def update_smoke_channel(self, param_name):
        self.update_channel("smoke", param_name)

    def create_opacity_channel(self, param_name):
            self.create_channel("Opacity Channel", "opacity", param_name)

    def update_opacity_channel(self, param_name):
        self.update_channel("opacity", param_name)

    def create_fire_channel(self, param_name):
        self.create_channel("Emission Channel", "fire", param_name)

    def update_fire_channel(self, param_name):
        self.update_channel("fire", param_name)

    @staticmethod
    def press_vdb_path(param_name):
        basic_filter = "OpenVDB File(*.vdb)"
        project_dir = pm.workspace(query=True, directory=True)
        vdb_path = pm.fileDialog2(fileFilter=basic_filter, cap="Select OpenVDB File", okc="Load", fm=1, startingDirectory=project_dir)
        if vdb_path is not None and len(vdb_path) > 0:
            vdb_path = vdb_path[0]
            # inspect file and try to figure out the padding and such
            try:
                dirname, filename = os.path.split(vdb_path)
                if re.match(".*[\._][0-9]+[\._]vdb", filename):
                    m = re.findall("[0-9]+", filename)
                    frame_number = m[-1]
                    padding = len(frame_number)
                    cache_start = int(frame_number)
                    cache_end = int(frame_number)
                    frame_location = filename.rfind(frame_number)
                    check_file_re = re.compile((filename[:frame_location] + "[0-9]{%i}" % padding + filename[frame_location + padding:]).replace(".", "\\."))
                    for each in os.listdir(dirname):
                        if os.path.isfile(os.path.join(dirname, each)):
                            if check_file_re.match(each):
                                current_frame_number = int(each[frame_location : frame_location + padding])
                                cache_start = min(cache_start, current_frame_number)
                                cache_end = max(cache_end, current_frame_number)
                    frame_location = vdb_path.rfind(frame_number)
                    vdb_path = vdb_path[:frame_location] + "#" * padding + vdb_path[frame_location + padding:]
                    node_name = param_name.split(".")[0]
                    pm.setAttr("%s.cache_playback_start" % node_name, cache_start)
                    pm.setAttr("%s.cache_playback_end" % node_name, cache_end)
            except:
                print "[openvdb] Error while trying to figure out padding, and frame range!"
                import sys, traceback
                traceback.print_exc(file=sys.stdout)
            pm.textFieldButtonGrp("OpenVDBPathGrp", edit=True, text=vdb_path)
            pm.setAttr(param_name, vdb_path, type="string")

    def create_vdb_path(self, param_name):
        pm.setUITemplate("attributeEditorPresetsTemplate", pushTemplate=True)
        pm.textFieldButtonGrp("OpenVDBPathGrp", label="VDB Path", buttonLabel="...")
        self.update_vdb_path(param_name)
        pm.setUITemplate(popTemplate=True)

    def update_vdb_path(self, param_name):
        vdb_path = pm.getAttr(param_name)
        pm.textFieldButtonGrp("OpenVDBPathGrp", edit=True, text="" if vdb_path is None else vdb_path,
                              changeCommand=lambda val: pm.setAttr(param_name, val), buttonCommand=lambda: AEvdb_visualizerTemplate.press_vdb_path(param_name))

    def create_volume_cache_limit_slider(self, param_name):
        pm.floatSliderButtonGrp("VDBVisualizerVolumeCacheLimit", label="Volume Cache Limit (GB)", buttonLabel="Update",
                                field=True, columnWidth4=(144,70,0,20), columnAttach=(1,'right',5),
                                minValue=0, maxValue=10, precision=0, step=1, sliderStep=1, fieldStep=1)
        self.update_volume_cache_limit_slider(param_name)

    def update_volume_cache_limit_slider(self, param_name):
        cache_limit = CacheLimit()

        def button_command():
            maya.cmds.vdb_visualizer_volume_cache(edit=True, limit=cache_limit.get())
            maya.cmds.vdb_visualizer_volume_cache(limit=True)

        control = pm.floatSliderButtonGrp("VDBVisualizerVolumeCacheLimit", edit=True,
                                          value=cache_limit.get(),
                                          changeCommand=lambda val: cache_limit.set(round(val)),
                                          buttonCommand=button_command)
        control.dragCommand(lambda val: control.setValue(round(val)))

    def create_voxel_type_menu(self, param_name):
        def change_command(item):
            maya.cmds.vdb_visualizer_volume_cache(edit=True, voxelType=item)
            maya.cmds.vdb_visualizer_volume_cache(voxelType=True)

        menu = pm.optionMenuGrp("VDBVisualizerVolumeCacheVoxelType",
                               label="Volume Cache Precision",
                               changeCommand=change_command).menu()
        menu.addItems(["half", "float"])
        menu.setWidth(70)
        self.update_voxel_type_menu(param_name)

    def update_voxel_type_menu(self, param_name):
        menu = pm.optionMenuGrp("VDBVisualizerVolumeCacheVoxelType", edit=True).menu()
        menu.setValue(maya.cmds.vdb_visualizer_volume_cache(query=True, voxelType=True))

    def create_channel_stats(self, param_name):
        pm.text("OpenVDBChannelStats", label=pm.getAttr(param_name), align="left")

    def update_channel_stats(self, param_name):
         pm.text("OpenVDBChannelStats", edit=True, label=pm.getAttr(param_name))

    @staticmethod
    def add_additional_channel(param_name, group, grid_name):
        current_grids_str = pm.getAttr(param_name)
        current_grids = None if current_grids_str is None else current_grids_str.split(" ")
        if current_grids is not None:
            if grid_name not in current_grids:
                if len(current_grids) > 0:
                    grid = "%s %s" % (current_grids_str, grid_name)
                    pm.setAttr(param_name, grid, type="string")
                    pm.textFieldGrp(group, edit=True, text=grid)
                else:
                    pm.setAttr(param_name, grid_name)
                    pm.textFieldGrp(group, edit=True, text=grid_name)
        else:
            pm.setAttr(param_name, grid_name)
            pm.textFieldGrp(group, edit=True, text=grid_name)

    @staticmethod
    def setup_additional_channel_menus(popup, group, param_name):
        pm.popupMenu(popup, edit=True, deleteAllItems=True)
        grids = pm.getAttr("%s.gridNames" % param_name.split(".")[0]).split(" ")
        if grids is not None and len(grids) > 0:
            for grid in grids:
                pm.menuItem(label=grid, parent=popup, command='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.add_additional_channel("%s", "%s", "%s")' % (param_name, group, grid))

    def setup_additional_channel_popup(self, grp, param_name):
        self.clear_popups(grp)
        pm.popupMenu(parent=grp, postMenuCommand=lambda popup, popup_parent: AEvdb_visualizerTemplate.setup_additional_channel_menus(popup, popup_parent, param_name))

    def create_additional_channel_export(self, param_name):
        grp = "OpenVDBAdditionalChannel"
        pm.setUITemplate("attributeEditorPresetsTemplate", pushTemplate=True)
        pm.textFieldGrp(grp, label="Channel Export")
        self.update_additional_channel_export(param_name)
        pm.setUITemplate(popTemplate=True)

    def update_additional_channel_export(self, param_name):
        grp = "OpenVDBAdditionalChannel"
        attr_value = pm.getAttr(param_name)
        pm.textFieldGrp(grp, edit=True,
                        text="" if attr_value is None else attr_value,
                        changeCommand=lambda val: pm.setAttr(param_name, val))
        pm.scriptJob(parent=grp,
                     replacePrevious=True,
                     attributeChange=[param_name,
                                      lambda: pm.textFieldGrp(grp, edit=True,
                                                               text=pm.getAttr(param_name))])
        self.setup_additional_channel_popup(grp, param_name)

    @staticmethod
    def add_velocity_grid(param_name, group, grid_name):
        current_grids_str = pm.getAttr(param_name)
        current_grids = None if current_grids_str is None else current_grids_str.split(" ")
        if current_grids is not None:
            if grid_name not in current_grids:
                if len(current_grids) > 0:
                    grid = "%s %s" % (current_grids_str, grid_name)
                    pm.setAttr(param_name, grid, type="string")
                    pm.textFieldGrp(group, edit=True, text=grid)
                else:
                    pm.setAttr(param_name, grid_name)
                    pm.textFieldGrp(group, edit=True, text=grid_name)
        else:
            pm.setAttr(param_name, grid_name)
            pm.textFieldGrp(group, edit=True, text=grid_name)

    @staticmethod
    def setup_velocity_grid_menus(popup, group, param_name):
        pm.popupMenu(popup, edit=True, deleteAllItems=True)
        grids = pm.getAttr("%s.gridNames" % param_name.split(".")[0]).split(" ")
        if grids is not None and len(grids) > 0:
            for grid in grids:
                pm.menuItem(label=grid, parent=popup, command='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.add_velocity_grid("%s", "%s", "%s")' % (param_name, group, grid))

    def setup_velocity_grid_popup(self, grp, param_name):
        self.clear_popups(grp)
        pm.popupMenu(parent=grp, postMenuCommand=lambda popup, popup_parent: AEvdb_visualizerTemplate.setup_velocity_grid_menus(popup, popup_parent, param_name))

    def create_velocity_grid_export(self, param_name):
        grp = "OpenVDBVelocityGrids"
        pm.setUITemplate("attributeEditorPresetsTemplate", pushTemplate=True)
        pm.textFieldGrp(grp, label="Velocity Grids")
        self.update_velocity_grid_export(param_name)
        pm.setUITemplate(popTemplate=True)

    def update_velocity_grid_export(self, param_name):
        grp = "OpenVDBVelocityGrids"
        attr_value = pm.getAttr(param_name)
        pm.textFieldGrp(grp, edit=True,
                        text="" if attr_value is None else attr_value,
                        changeCommand=lambda val: pm.setAttr(param_name, val))
        pm.scriptJob(parent=grp,
                     replacePrevious=True,
                     attributeChange=[param_name,
                                      lambda: pm.textFieldGrp(grp, edit=True,
                                                              text=pm.getAttr(param_name))])
        self.setup_velocity_grid_popup(grp, param_name)

    def __init__(self, node_name):
        self.beginScrollLayout()

        self.beginLayout("Cache Parameters", collapse=False)

        self.callCustom(self.create_vdb_path, self.update_vdb_path, "vdbPath")
        self.addControl("cachePlaybackStart", label="Cache Start")
        self.addControl("cachePlaybackEnd", label="Cache End")
        self.addControl("cacheBeforeMode", label="Before")
        self.addControl("cacheAfterMode", label="After")
        self.addControl("cacheTime", label="Cache Time")
        self.addControl("cachePlaybackOffset", label="Cache Offset")

        self.beginLayout("Statistics", collapse=True)

        self.callCustom(self.create_channel_stats, self.update_channel_stats, "channel_stats")

        self.endLayout()

        self.endLayout()

        self.beginLayout("Display Parameters", collapse=False)

        self.addControl("displayMode", label="Display Mode")
        self.addControl("pointSize", label="Point Size")
        self.addControl("pointJitter", label="Point Jitter")
        self.addControl("pointSkip", label="Point Skip")
        self.addControl("pointSort", label="Point Sort")

        self.addSeparator()
        self.addControl("sliceCount", label="Slice Count")
        self.addControl("shadowGain", label="Shadow Gain")
        self.addControl("shadowSampleCount", label="Shadow Sample Count")

        self.addSeparator()
        self.callCustom(self.create_volume_cache_limit_slider, self.update_volume_cache_limit_slider, "dummy_attr_1")
        self.callCustom(self.create_voxel_type_menu, self.update_voxel_type_menu, "dummy_attr_2")

        self.endLayout()

        self.beginLayout("Render Parameters", collapse=False)
        self.addControl("overrideShader", label="Override Shader")
        self.addControl("shaderMode", label="Shader Mode")
        self.addControl("matte", label="Matte")
        self.addControl("boundsSlack", label="Bounds Slack")

        self.beginLayout("Visibility Parameters", collapse=False)
        self.addControl("primaryVisibility", label="Primary Visibility")
        self.addControl("castsShadows", label="Casts Shadows")
        self.addControl("visibleInDiffuse", label="Visible in Diffuse")
        self.addControl("visibleInReflections", label="Visible in Reflections")
        self.addControl("visibleInGlossy", label="Visible in Glossy")
        self.addControl("visibleInRefractions", label="Visible in Refractions")
        self.addControl("visibleInSubsurface", label="Visible in Subsurface")
        self.addControl("selfShadows", label="Self Shadows")
        self.endLayout()

        self.beginLayout("Sampling", collapse=False)
        self.addControl("position_offset")
        self.addControl("interpolation")
        self.addControl("compensate_scaling", label="Compensate for Scaling")
        self.addControl("sampling_quality", label="Sampling Quality")
        self.endLayout()

        self.beginLayout("Velocity", collapse=True)
        self.callCustom(self.create_velocity_grid_export, self.update_velocity_grid_export, "velocityGrids")
        self.addControl("velocityScale", label="Scale")
        self.addControl("velocityFps", label="FPS")
        self.addControl("velocityShutterStart", label="Shutter Start")
        self.addControl("velocityShutterEnd", label="Shutter End")
        self.endLayout()

        self.beginLayout("Simple Shader", collapse=True)
        self.beginLayout("Color", collapse=False)
        self.addControl("smoke", label="Color")
        self.callCustom(self.create_smoke_channel, self.update_smoke_channel, "smoke_channel")
        self.addControl("smokeIntensity", label="Intensity")
        self.addControl("anisotropy", label="Anisotropy")
        self.create_gradient_params("simpleSmoke", node_name)
        self.endLayout()

        self.beginLayout("Opacity", collapse=False)
        self.addControl("opacity", label="Opacity")
        self.callCustom(self.create_opacity_channel, self.update_opacity_channel, "opacity_channel")
        self.addControl("opacityIntensity", label="Intensity")
        self.addControl("opacityShadow", label="Shadow Multiplier")
        self.create_gradient_params("simpleOpacity", node_name)
        self.endLayout()

        self.beginLayout("Emission", collapse=False)
        self.endLayout()
        self.addControl("fire", label="Emission")
        self.callCustom(self.create_fire_channel, self.update_fire_channel, "fire_channel")
        self.addControl("fireIntensity", label="Intensity")
        self.create_gradient_params("simpleFire", node_name)
        self.endLayout()

        self.beginLayout("Standard Volume shader", collapse=True)

        def AddRamp(ramp_name):
            pm.mel.eval('source AEaddRampControl.mel; AEaddRampControl("%s.%s")' % (node_name, ramp_name))

        self.beginLayout("Density", collapse=False)
        self.addControl("sv_density", label="Density")
        self.add_channel_control("Density Channel", "StandardVolumeDensity", "sv_density_channel")
        self.addControl("sv_density_source", label="Channel Mode")
        AddRamp("svDensityRamp")
        self.endLayout()

        self.beginLayout("Scatter", collapse=False)
        self.addControl("sv_scatter", label="Weight")
        self.addControl("sv_scatter_color", label="Color")
        self.add_channel_control("Color Channel", "StandardVolumeScatter",  "sv_scatter_color_channel")
        self.addControl("sv_scatter_color_source", label="Channel Mode")
        self.addControl("sv_scatter_anisotropy", label="Anisotropy")
        AddRamp("svScatterColorRamp")
        self.endLayout()

        self.beginLayout("Transparent", collapse=False)
        self.addControl("sv_transparent", label="Weight")
        self.add_channel_control("Channel", "StandardVolumeTransparent", "sv_transparent_channel")
        self.endLayout()

        self.beginLayout("Emission", collapse=False)
        self.addControl("sv_emission_mode", label="Mode")
        self.addControl("sv_emission", label="Weight")
        self.addControl("sv_emission_color", label="Color")
        self.add_channel_control("Channel", "StandardVolumeEmission", "sv_emission_channel")
        self.addControl("sv_emission_source", label="Channel Mode")
        AddRamp("svEmissionRamp")
        self.endLayout()

        self.beginLayout("Temperature", collapse=False)
        self.addControl("sv_temperature", label="Temperature")
        self.add_channel_control("Channel", "StandardVolumeTemperature", "sv_temperature_channel")
        self.addControl("sv_blackbody_kelvin", label="Blackbody Kelvin")
        self.addControl("sv_blackbody_intensity", label="Blackbody Intensity")
        self.endLayout()

        self.endLayout()

        self.endLayout()

        self.endLayout()

        self.beginLayout("Overrides", collapse=True)
        self.callCustom(self.create_additional_channel_export, self.update_additional_channel_export, "additional_channel_export")
        self.endLayout()

        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
