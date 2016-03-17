import pymel.core as pm

def node_attr(node_name, attr_name):
    return '%s.%s' % (node_name, attr_name)

class AEvdb_visualizerTemplate(pm.uitypes.AETemplate):
    def scattering_source(self, node_name):
        if pm.getAttr(node_attr(node_name, 'scattering_source')) == 0:
            pm.editorTemplate(dimControl=(node_name, 'scattering', False))
            pm.editorTemplate(dimControl=(node_name, 'scatteringChannel', True))
        else:
            pm.editorTemplate(dimControl=(node_name, 'scattering', True))
            pm.editorTemplate(dimControl=(node_name, 'scatteringChannel', False))

    def attenuation_source(self, node_name):
        source_val = pm.getAttr(node_attr(node_name, 'attenuation_source'))
        if source_val == 0:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', False))
            pm.editorTemplate(dimControl=(node_name, 'attenuationChannel', True))
        elif source_val == 1:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', True))
            pm.editorTemplate(dimControl=(node_name, 'attenuationChannel', False))
        else:
            pm.editorTemplate(dimControl=(node_name, 'attenuation', True))
            pm.editorTemplate(dimControl=(node_name, 'attenuationChannel', True))

    def emission_source(self, node_name):
        if pm.getAttr(node_attr(node_name, 'emission_source')) == 0:
            pm.editorTemplate(dimControl=(node_name, 'emission', False))
            pm.editorTemplate(dimControl=(node_name, 'emissionChannel', True))
        else:
            pm.editorTemplate(dimControl=(node_name, 'emission', True))
            pm.editorTemplate(dimControl=(node_name, 'emissionChannel', False))

    def __init__(self, node_name):
        self.beginScrollLayout()

        self.beginLayout('Cache Parameters', collapse=False)

        self.addControl('vdbPath', label='Cache Path')
        self.addControl('cachePlaybackStart', label='Playback Start')
        self.addControl('cachePlaybackEnd', label='Playback End')
        self.addControl('cacheBeforeMode', label='Before')
        self.addControl('cacheAfterMode', label='After')
        self.addControl('cachePlaybackOffset', label='Playback Offset')

        self.endLayout()

        self.beginLayout('Display Parameters', collapse=False)

        self.addControl('displayMode')

        self.endLayout()

        self.beginLayout('Render Parameters', collapse=False)

        self.beginLayout('Scattering', collapse=False)
        self.addControl('scattering_source', changeCommand=self.scattering_source)
        self.addControl('scattering', label='Scattering')
        self.addControl('scattering_channel')
        self.addControl('scattering_color')
        self.addControl('scattering_intensity')
        self.addControl('anisotropy')
        self.endLayout()

        self.beginLayout('Attenuation', collapse=False)
        self.addControl('attenuation_source', changeCommand=self.attenuation_source)
        self.addControl('attenuation', label='Attenuation')
        self.addControl('attenuation_channel')
        self.addControl('attenuation_color')
        self.addControl('attenuation_intensity')
        self.addControl('attenuation_mode', label='Attenuation Mode')
        self.endLayout()

        self.beginLayout('Emission', collapse=False)
        self.addControl('emission_source', changeCommand=self.emission_source)
        self.addControl('emission', label='Emission')
        self.addControl('emission_channel')
        self.addControl('emission_color')
        self.addControl('emission_intensity')
        self.endLayout()

        self.beginLayout('Sampling', collapse=False)
        self.addControl('position_offset')
        self.addControl('interpolation')
        self.addControl('compensate_scaling', label='Compensate for Scaling')
        self.endLayout()

        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()

