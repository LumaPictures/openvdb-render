import pymel.core as pm

from channelController import channelController

class AEvdb_shaderTemplate(pm.uitypes.AETemplate, channelController):
    def __init__(self, node_name):
        self.beginScrollLayout()

        self.beginLayout('Scattering', collapse=False)
        self.addControl('scattering_source', label='Source')
        self.addControl('scattering', label='Scattering')
        self.addControl('scattering_channel', label='Scattering Channel')
        self.create_gradient_params('scattering', node_name)
        self.addControl('scattering_color', label='Color')
        self.addControl('scattering_intensity', label='Intensity')
        self.addControl('anisotropy')
        self.endLayout()

        self.beginLayout('Attenuation', collapse=False)
        self.addControl('attenuation_source')
        self.addControl('attenuation', label='Attenuation')
        self.addControl('attenuation_channel', label='Channel')
        self.create_gradient_params('attenuation', node_name)
        self.addControl('attenuation_color', label='Color')
        self.addControl('attenuation_intensity', label='Intensity')
        self.addControl('attenuation_mode', label='Mode')
        self.endLayout()

        self.beginLayout('Emission', collapse=False)
        self.addControl('emission_source', label='Source')
        self.addControl('emission', label='Emission')
        self.addControl('emission_channel', label='Channel')
        self.create_gradient_params('emission', node_name)
        self.addControl('emission_color', label='Color')
        self.addControl('emission_intensity', label='Intensity')
        self.endLayout()

        self.beginLayout('Sampling Parameters', collapse=False)
        self.addControl('positionOffset', label='Position Offset')
        self.addControl('interpolation', label='Interpolation')
        self.addControl('compensate_scaling', label='Compensate for Scaling')
        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
