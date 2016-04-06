import pymel.core as pm

class channelController:
    @staticmethod
    def build_channel_frame_layout(channel, frame_layout, param_name):
        if frame_layout is '':
            return
        childs = pm.frameLayout(frame_layout, query=True, childArray=True)
        if childs != None and isinstance(childs, list) and len(childs) > 0:
            for child in childs:
                try:
                    pm.deleteUI(child)
                except:
                    pass
        pm.columnLayout(parent=frame_layout)
        channel_mode = pm.getAttr(param_name)
        node_name = param_name.split('.')[0]
        if channel_mode == 1: # float
            pm.attrControlGrp(attribute='%s.%sInputMin' % (node_name, channel), label='Input Min')
            pm.attrControlGrp(attribute='%s.%sInputMax' % (node_name, channel), label='Input Max')
            pm.attrControlGrp(attribute='%s.%sContrast' % (node_name, channel), label='Contrast')
            pm.attrControlGrp(attribute='%s.%sContrastPivot' % (node_name, channel), label='Contrast Pivot')
            pm.attrControlGrp(attribute='%s.%sBias' % (node_name, channel), label='Bias')
            pm.attrControlGrp(attribute='%s.%sGain' % (node_name, channel), label='Gain')
            pm.attrControlGrp(attribute='%s.%sOutputMin' % (node_name, channel), label='Output Min')
            pm.attrControlGrp(attribute='%s.%sOutputMax' % (node_name, channel), label='Output Max')
            pm.attrControlGrp(attribute='%s.%sClampMin' % (node_name, channel), label='Clamp Min')
            pm.attrControlGrp(attribute='%s.%sClampMax' % (node_name, channel), label='Clamp Max')
        elif channel_mode == 2: # rgb
            pm.attrControlGrp(attribute='%s.%sGamma' % (node_name, channel), label='Gamma')
            pm.attrControlGrp(attribute='%s.%sHueShift' % (node_name, channel), label='Hue Shift')
            pm.attrControlGrp(attribute='%s.%sSaturation' % (node_name, channel), label='Saturation')
            pm.attrControlGrp(attribute='%s.%sContrast' % (node_name, channel), label='Contrast')
            pm.attrControlGrp(attribute='%s.%sContrastPivot' % (node_name, channel), label='Contrast Pivot')
            pm.attrControlGrp(attribute='%s.%sExposure' % (node_name, channel), label='Exposure')
            pm.attrControlGrp(attribute='%s.%sMultiply' % (node_name, channel), label='Multiply')
            pm.attrControlGrp(attribute='%s.%sAdd' % (node_name, channel), label='Add')
        elif channel_mode == 3: # float to float
            pm.attrControlGrp(attribute='%s.%sInputMin' % (node_name, channel), label='Input Min')
            pm.attrControlGrp(attribute='%s.%sInputMax' % (node_name, channel), label='Input Max')
            pm.mel.eval('source AEaddRampControl.mel; AEmakeLargeRamp("%s", 0, 0, 0, 0, 0)' % ('%s.%sFloatRamp' % (node_name, channel)))
            pm.attrControlGrp(attribute='%s.%sContrast' % (node_name, channel), label='Contrast')
            pm.attrControlGrp(attribute='%s.%sContrastPivot' % (node_name, channel), label='Contrast Pivot')
            pm.attrControlGrp(attribute='%s.%sBias' % (node_name, channel), label='Bias')
            pm.attrControlGrp(attribute='%s.%sGain' % (node_name, channel), label='Gain')
            pm.attrControlGrp(attribute='%s.%sOutputMin' % (node_name, channel), label='Output Min')
            pm.attrControlGrp(attribute='%s.%sOutputMax' % (node_name, channel), label='Output Max')
            pm.attrControlGrp(attribute='%s.%sClampMin' % (node_name, channel), label='Clamp Min')
            pm.attrControlGrp(attribute='%s.%sClampMax' % (node_name, channel), label='Clamp Max')
        elif channel_mode == 4: # float to rgb
            pm.attrControlGrp(attribute='%s.%sInputMin' % (node_name, channel), label='Input Min')
            pm.attrControlGrp(attribute='%s.%sInputMax' % (node_name, channel), label='Input Max')
            pm.mel.eval('source AEaddRampControl.mel; AEmakeLargeRamp("%s", 0, 0, 0, 0, 0)' % ('%s.%sRgbRamp' % (node_name, channel)))
            pm.attrControlGrp(attribute='%s.%sGamma' % (node_name, channel), label='Gamma')
            pm.attrControlGrp(attribute='%s.%sHueShift' % (node_name, channel), label='Hue Shift')
            pm.attrControlGrp(attribute='%s.%sSaturation' % (node_name, channel), label='Saturation')
            pm.attrControlGrp(attribute='%s.%sContrast' % (node_name, channel), label='Contrast')
            pm.attrControlGrp(attribute='%s.%sContrastPivot' % (node_name, channel), label='Contrast Pivot')
            pm.attrControlGrp(attribute='%s.%sExposure' % (node_name, channel), label='Exposure')
            pm.attrControlGrp(attribute='%s.%sMultiply' % (node_name, channel), label='Multiply')
            pm.attrControlGrp(attribute='%s.%sAdd' % (node_name, channel), label='Add')
        pm.setParent('..')

    def create_gradient(self, param_name):
        channel = param_name.split('.')[1].replace('_channel_mode', '')
        pm.setUITemplate('attributeEditorPresetsTemplate', pushTemplate=True)
        setattr(self, '%s_channel_mode' % channel, pm.attrControlGrp(attribute=param_name))
        setattr(self, '%s_frame_layout' % channel, pm.frameLayout(collapse=True, collapsable=True, label='Channel Controls'))
        pm.setUITemplate(popTemplate=True)
        self.update_gradient(param_name)

    def update_gradient(self, param_name):
        channel = param_name.split('.')[1].replace('_channel_mode', '')
        channel_mode = getattr(self, '%s_channel_mode' % channel)
        frame_layout = getattr(self, '%s_frame_layout' % channel)
        channelController.build_channel_frame_layout(channel, frame_layout, param_name)
        pm.attrControlGrp(channel_mode, edit=True, attribute=param_name, changeCommand='import channelController; channelController.channelController.build_channel_frame_layout("%s", "%s", "%s")' % (channel, frame_layout, param_name))

    def init_gradient_params(self, channel):
        setattr(self, '%s_channel_mode' % channel, '')
        setattr(self, '%s_frame_layout' % channel, '')
