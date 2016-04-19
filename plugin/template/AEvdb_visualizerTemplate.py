import pymel.core as pm
import re, os

from channelController import channelController

class AEvdb_visualizerTemplate(pm.uitypes.AETemplate, channelController):
    @staticmethod
    def setup_popup_menu_elems(pup, param_name):
        pm.popupMenu(pup, edit=True, deleteAllItems=True)
        grid_names_str = pm.getAttr('%s.gridNames' % param_name.split('.')[0])
        if grid_names_str is not None and len(grid_names_str) > 0:
            for each in grid_names_str.split(' '):
                pm.menuItem(label=each, parent=pup, command='pm.setAttr("%s", "%s", type="string")' % (param_name, each))

    def setup_popup_menu(self, pup, param_name):
        pm.popupMenu(pup, edit=True, postMenuCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.setup_popup_menu_elems("%s", "%s")' % (pup, param_name))

    def create_channel(self, annotation, channel_name, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        pm.attrControlGrp('OpenVDB%sChannelGrp' % channel_name, annotation=annotation, attribute=param_name)
        self.update_channel(channel_name, param_name)
        pm.setUITemplate(popTemplate=True)

    def update_channel(self, channel_name, param_name):
        grp = 'OpenVDB%sChannelGrp' % channel_name
        pm.attrControlGrp(grp, edit=True, attribute=param_name)
        pup = 'OpenVDB%sChannelPopup' % channel_name
        try:
            pm.deleteUI(pup)
        except:
            pass
        pm.popupMenu(pup, parent=grp)
        self.setup_popup_menu(pup, param_name)

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
    def change_vdb_path(param_name):
        pm.setAttr(param_name, pm.textFieldButtonGrp('OpenVDBPathGrp', query=True, text=True), type='string')

    @staticmethod
    def press_vdb_path(param_name):
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
            pm.textFieldButtonGrp('OpenVDBPathGrp', edit=True, text=vdb_path)
            pm.setAttr(param_name, vdb_path, type='string')

    def create_vdb_path(self, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        pm.textFieldButtonGrp('OpenVDBPathGrp', label='VDB Path', buttonLabel='...')
        self.update_vdb_path(param_name)
        pm.setUITemplate(popTemplate=True)

    def update_vdb_path(self, param_name):
        pm.textFieldButtonGrp('OpenVDBPathGrp', edit=True, text=pm.getAttr(param_name),
                              changeCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.change_vdb_path("%s")' % param_name, buttonCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.press_vdb_path("%s")' % param_name)

    def create_channel_stats(self, param_name):
        pm.text('OpenVDBChannelStats', label=pm.getAttr(param_name), align='left')

    def update_channel_stats(self, param_name):
         pm.text('OpenVDBChannelStats', edit=True, label=pm.getAttr(param_name))

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
    def setup_additional_channel_menus(param_name):
        pm.popupMenu('OpenVDBAdditionalChannelPopup', edit=True, deleteAllItems=True)
        grids = pm.getAttr('%s.gridNames' % param_name.split('.')[0]).split(' ')
        if grids is not None and len(grids) > 0:
            for grid in grids:
                pm.menuItem(label=grid, parent='OpenVDBAdditionalChannelPopup', command='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.add_additional_channel("%s", "%s")' % (param_name, grid))

    def setup_additional_channel_popup(self, param_name):
        try:
            pm.deleteUI('OpenVDBAdditionalChannelPopup')
        except:
            pass
        pm.popupMenu('OpenVDBAdditionalChannelPopup', parent='OpenVDBAdditionalChannel')
        pm.popupMenu('OpenVDBAdditionalChannelPopup', edit=True, postMenuCommand='import AEvdb_visualizerTemplate; AEvdb_visualizerTemplate.AEvdb_visualizerTemplate.setup_additional_channel_menus("%s")' % param_name)

    def create_additional_channel_export(self, param_name):
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        pm.attrControlGrp('OpenVDBAdditionalChannel', annotation='Channel Export', attribute=param_name)
        pm.setUITemplate(popTemplate=True)
        self.update_additional_channel_export(param_name)

    def update_additional_channel_export(self, param_name):
        pm.attrControlGrp('OpenVDBAdditionalChannel', edit=True, attribute=param_name)
        self.setup_additional_channel_popup(param_name)

    def __init__(self, node_name):
        self.beginScrollLayout()

        self.beginLayout('Cache Parameters', collapse=False)

        self.callCustom(self.create_vdb_path, self.update_vdb_path, 'vdbPath')
        self.addControl('cachePlaybackStart', label='Cache Start')
        self.addControl('cachePlaybackEnd', label='Cache End')
        self.addControl('cacheBeforeMode', label='Before')
        self.addControl('cacheAfterMode', label='After')
        self.addControl('cacheTime', label='Cache Time')
        self.addControl('cachePlaybackOffset', label='Cache Offset')

        self.beginLayout('Statistics', collapse=True)

        self.callCustom(self.create_channel_stats, self.update_channel_stats, 'channel_stats')

        self.endLayout()

        self.endLayout()

        self.beginLayout('Display Parameters', collapse=False)

        self.addControl('displayMode', label='Display Mode')
        self.addControl('pointSize', label='Point Size')
        self.addControl('pointJitter', label='Point Jitter')
        self.addControl('pointSkip', label='Point Skip')

        self.endLayout()

        self.beginLayout('Render Parameters', collapse=False)

        self.beginLayout('Sampling', collapse=False)
        self.addControl('position_offset')
        self.addControl('interpolation')
        self.addControl('compensate_scaling', label='Compensate for Scaling')
        self.addControl('sampling_quality', label='Sampling Quality')
        self.endLayout()

        self.beginLayout('Scattering', collapse=False)
        self.addControl('scattering_source')
        self.addControl('scattering', label='Scattering')
        self.callCustom(self.create_scattering_channel, self.update_scattering_channel, 'scattering_channel')
        self.create_gradient_params('scattering', node_name)
        self.addControl('scattering_color')
        self.addControl('scattering_intensity')
        self.addControl('anisotropy')
        self.endLayout()

        self.beginLayout('Attenuation', collapse=False)
        self.addControl('attenuation_source')
        self.addControl('attenuation', label='Attenuation')
        self.callCustom(self.create_attenuation_channel, self.update_attenuation_channel, 'attenuation_channel')
        self.create_gradient_params('attenuation', node_name)
        self.addControl('attenuation_color')
        self.addControl('attenuation_intensity')
        self.addControl('attenuation_mode', label='Attenuation Mode')
        self.endLayout()

        self.beginLayout('Emission', collapse=False)
        self.addControl('emission_source')
        self.addControl('emission', label='Emission')
        self.callCustom(self.create_emission_channel, self.update_emission_channel, 'emission_channel')
        self.create_gradient_params('emission', node_name)
        self.addControl('emission_color')
        self.addControl('emission_intensity')
        self.endLayout()

        self.beginLayout('Overrides', collapse=True)
        self.callCustom(self.create_additional_channel_export, self.update_additional_channel_export, 'additional_channel_export')
        self.endLayout()

        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
