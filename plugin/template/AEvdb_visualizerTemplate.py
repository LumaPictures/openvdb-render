import pymel.core as pm
import re, os

class AEvdb_visualizerTemplate(pm.uitypes.AETemplate):
    def scattering_source(self, node_name):
        if pm.getAttr('%s.scattering_source' % node_name) == 0:
            pm.editorTemplate(dimControl=(node_name, 'scattering', False))
            if self.scattering_channel_grp != '':
                pm.attrControlGrp(self.scattering_channel_grp, edit=True, enable=False)
        else:
            pm.editorTemplate(dimControl=(node_name, 'scattering', True))
            if self.scattering_channel_grp != '':
                pm.attrControlGrp(self.scattering_channel_grp, edit=True, enable=True)

    def attenuation_source(self, node_name):
        source_val = pm.getAttr('%s.attenuation_source' % node_name)
        if source_val == 0:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', False))
            if self.attenuation_channel_grp != '':
                pm.attrControlGrp(self.attenuation_channel_grp, edit=True, enable=False)
        elif source_val == 1:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', True))
            if self.attenuation_channel_grp != '':
                pm.attrControlGrp(self.attenuation_channel_grp, edit=True, enable=True)
        else:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', True))
            if self.attenuation_channel_grp != '':
                pm.attrControlGrp(self.attenuation_channel_grp, edit=True, enable=False)

    def emission_source(self, node_name):
        if pm.getAttr('%s.emission_source' % node_name) == 0:
            pm.editorTemplate(dimControl=(node_name, 'emission', False))
            if self.emission_channel_grp != '':
                pm.attrControlGrp(self.emission_channel_grp, edit=True, enable=False)
        else:
            pm.editorTemplate(dimControl=(node_name, 'emission', True))
            if self.emission_channel_grp != '':
                pm.attrControlGrp(self.emission_channel_grp, edit=True, enable=True)

    @staticmethod
    def setup_popup_menu_elems(parent_ui, param_name):
        pm.popupMenu(parent_ui, edit=True, deleteAllItems=True)
        grid_names_str = pm.getAttr('%s.gridNames' % param_name.split('.')[0])
        if grid_names_str is not None and len(grid_names_str) > 0:
            for each in grid_names_str.split(' '):
                pm.menuItem(label=each, parent=parent_ui, command='pm.setAttr("%s", "%s", type="string")' % (param_name, each))

    def setup_popup_menu(self, parent_ui, param_name, pup):
        if pup == '':
            pup = pm.popupMenu(parent=parent_ui)
        pm.popupMenu(pup, edit=True, postMenuCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.setup_popup_menu_elems("%s", "%s")' % (pup, param_name))
        return pup

    def create_channel(self, annotation, channel_name, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        grp = pm.attrControlGrp(annotation=annotation, attribute=param_name)
        setattr(self, '%s_channel_grp' % channel_name, grp)
        self.update_channel(channel_name, param_name)
        pm.setUITemplate(popTemplate=True)

    def update_channel(self, channel_name, param_name):
        grp = getattr(self, '%s_channel_grp' % channel_name)
        pm.attrControlGrp(grp, edit=True, attribute=param_name)
        pup_name = '%s_channel_popup' % channel_name
        setattr(self, pup_name, self.setup_popup_menu(grp, param_name, getattr(self, pup_name)))

    # TODO : maybe use something like templates in c++?
    def create_scattering_channel(self, param_name):
        self.create_channel('Scattering Channel', 'scattering', param_name)

    def update_scattering_channel(self, param_name):
        self.update_channel('scattering', param_name)

    def create_attenuation_channel(self, param_name):
        self.create_channel('Attenuation Channel', 'attenuation', param_name)

    def update_attenuation_channel(self, param_name):
        self.update_channel('attenuation', param_name)

    def create_emission_channel(self, param_name):
        self.create_channel('Emission Channel', 'emission', param_name)

    def update_emission_channel(self, param_name):
        self.update_channel('emission', param_name)

    @staticmethod
    def change_vdb_path(grp, param_name):
        pm.setAttr(param_name, pm.textFieldButtonGrp(grp, query=True, text=True), type='string')

    @staticmethod
    def press_vdb_path(grp, param_name):
        basic_filter = 'OpenVDB File(*.vdb)'
        project_dir = pm.workspace(query=True, directory=True)
        vdb_path = pm.fileDialog2(fileFilter=basic_filter, cap='Select OpenVDB File', okc='Load', fm=1, startingDirectory=project_dir)
        if vdb_path is not None and len(vdb_path) > 0:
            vdb_path = vdb_path[0]
            # inspect file and try to figure out the padding and such
            try:
                dirname, filename = os.path.split(vdb_path)
                if re.match('.*[\._][0-9]+[\._]vdb', filename):
                    m = re.findall('[0-9]+', filename)
                    frame_number = m[-1]
                    padding = len(frame_number)
                    cache_start = int(frame_number)
                    cache_end = int(frame_number)
                    frame_location = filename.rfind(frame_number)
                    check_file_re = re.compile((filename[:frame_location] + '[0-9]{%i}' % padding + filename[frame_location + padding:]).replace('.', '\\.'))
                    for each in os.listdir(dirname):
                        if os.path.isfile(os.path.join(dirname, each)):
                            if check_file_re.match(each):
                                current_frame_number = int(each[frame_location : frame_location + padding])
                                cache_start = min(cache_start, current_frame_number)
                                cache_end = max(cache_end, current_frame_number)
                    frame_location = vdb_path.rfind(frame_number)
                    vdb_path = vdb_path[:frame_location] + '#' * padding + vdb_path[frame_location + padding:]
                    node_name = param_name.split('.')[0]
                    pm.setAttr('%s.cache_playback_start' % node_name, cache_start)
                    pm.setAttr('%s.cache_playback_end' % node_name, cache_end)
            except:
                print '[openvdb] Error while trying to figure out padding, and frame range!'
                import sys, traceback
                traceback.print_exc(file=sys.stdout)
            pm.textFieldButtonGrp(grp, edit=True, text=vdb_path)
            pm.setAttr(param_name, vdb_path, type='string')

    def create_vdb_path(self, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        self.vdb_path_grp = pm.textFieldButtonGrp(label='VDB Path', buttonLabel='...')
        self.update_vdb_path(param_name)
        pm.setUITemplate(popTemplate=True)

    def update_vdb_path(self, param_name):
        pm.textFieldButtonGrp(self.vdb_path_grp, edit=True, text=pm.getAttr(param_name),
                              changeCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.change_vdb_path("%s", "%s")' % (self.vdb_path_grp, param_name), buttonCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.press_vdb_path("%s", "%s")' % (self.vdb_path_grp, param_name))

    def create_channel_stats(self, param_name):
        self.channel_stats = pm.text(label=pm.getAttr(param_name), align='left')

    def update_channel_stats(self, param_name):
         pm.text(self.channel_stats, edit=True, label=pm.getAttr(param_name))

    @staticmethod
    def add_additional_channel(param_name, grid_name):
        current_grids_str = pm.getAttr(param_name)
        current_grids = None if current_grids_str is None else current_grids_str.split(' ')
        if current_grids is not None:
            if grid_name not in current_grids:
                if len(current_grids) > 0:
                    pm.setAttr(param_name, '%s %s' % (current_grids_str, grid_name), type='string')
                else:
                    pm.setAttr(param_name, grid_name)
        else:
            pm.setAttr(param_name, grid_name)

    @staticmethod
    def setup_additional_channel_menus(param_name, pup):
        pm.popupMenu(pup, edit=True, deleteAllItems=True)
        grids = pm.getAttr('%s.gridNames' % param_name.split('.')[0]).split(' ')
        if grids is not None and len(grids) > 0:
            for grid in grids:
                pm.menuItem(label=grid, parent=pup, command='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.add_additional_channel("%s", "%s")' % (param_name, grid))

    def setup_additional_channel_popup(self, param_name):
        if self.additional_channel_export_popup == '':
            self.additional_channel_export_popup = pm.popupMenu(parent=self.additional_channel_export)
        pm.popupMenu(self.additional_channel_export_popup, edit=True, postMenuCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.setup_additional_channel_menus("%s", "%s")' % (param_name, self.additional_channel_export_popup))

    def create_additional_channel_export(self, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        self.additional_channel_export = pm.attrControlGrp(annotation='Channel Export', attribute=param_name)
        pm.setUITemplate(popTemplate=True)
        self.update_additional_channel_export(param_name)

    def update_additional_channel_export(self, param_name):
        pm.attrControlGrp(self.additional_channel_export, edit=True, attribute=param_name)
        self.setup_additional_channel_popup(param_name)

    def create_gradient(self, param_name):
        param_name_splits = param_name.split('.')
        #node_name = param_name_splits[0]
        channel_name = param_name_splits[1].replace('_channel_mode', '')
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        setattr(self, '%s_channel_mode' % channel_name, pm.attrControlGrp(attribute=param_name))
        pm.setUITemplate(popTemplate=True)

    def update_gradient(self, param_name):
        pass

    def __init__(self, node_name):
        for each in ['scattering', 'attenuation', 'emission']:
            setattr(self, '%s_channel_grp' % each, '')
            setattr(self, '%s_channel_popup' % each, '')
            setattr(self, '%s_gradient_type' % each, '')

        self.vdb_path_grp = ''
        self.channel_stats = ''
        self.additional_channel_export = ''
        self.additional_channel_export_popup = ''

        self.beginScrollLayout()

        self.beginLayout('Cache Parameters', collapse=False)

        self.callCustom(self.create_vdb_path, self.update_vdb_path, 'vdbPath')
        self.addControl('cachePlaybackStart', label='Cache Start')
        self.addControl('cachePlaybackEnd', label='Cache End')
        self.addControl('cacheBeforeMode', label='Before')
        self.addControl('cacheAfterMode', label='After')
        self.addControl('cachePlaybackOffset', label='Cache Offset')

        self.beginLayout('Statistics', collapse=True)

        self.callCustom(self.create_channel_stats, self.update_channel_stats, 'channel_stats')

        self.endLayout()

        self.endLayout()

        self.beginLayout('Display Parameters', collapse=False)

        self.addControl('displayMode')

        self.endLayout()

        self.beginLayout('Render Parameters', collapse=False)

        self.beginLayout('Scattering', collapse=False)
        self.addControl('scattering_source', changeCommand=self.scattering_source)
        self.addControl('scattering', label='Scattering')
        self.callCustom(self.create_scattering_channel, self.update_scattering_channel, 'scattering_channel')
        self.addControl('scattering_color')
        self.addControl('scattering_intensity')
        self.addControl('anisotropy')
        self.callCustom(self.create_gradient, self.update_gradient, 'scattering_channel_mode')
        self.endLayout()

        self.beginLayout('Attenuation', collapse=False)
        self.addControl('attenuation_source', changeCommand=self.attenuation_source)
        self.addControl('attenuation', label='Attenuation')
        self.callCustom(self.create_attenuation_channel, self.update_attenuation_channel, 'attenuation_channel')
        self.addControl('attenuation_color')
        self.addControl('attenuation_intensity')
        self.addControl('attenuation_mode', label='Attenuation Mode')
        self.callCustom(self.create_gradient, self.update_gradient, 'attenuation_channel_mode')
        self.endLayout()

        self.beginLayout('Emission', collapse=False)
        self.addControl('emission_source', changeCommand=self.emission_source)
        self.addControl('emission', label='Emission')
        self.callCustom(self.create_emission_channel, self.update_emission_channel, 'emission_channel')
        self.addControl('emission_color')
        self.addControl('emission_intensity')
        self.callCustom(self.create_gradient, self.update_gradient, 'emission_channel_mode')
        self.endLayout()

        self.beginLayout('Sampling', collapse=False)
        self.addControl('position_offset')
        self.addControl('interpolation')
        self.addControl('compensate_scaling', label='Compensate for Scaling')
        self.endLayout()

        self.beginLayout('Overrides', collapse=True)
        self.callCustom(self.create_additional_channel_export, self.update_additional_channel_export, 'additional_channel_export')
        self.endLayout()

        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
