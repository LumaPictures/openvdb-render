import pymel.core as pm

from channelController import channelController

class AEvdb_samplerTemplate(pm.uitypes.AETemplate, channelController):
    def __init__(self, node_name):
        self.init_gradient_params("base")

        self.beginScrollLayout()

        self.beginLayout('Channel Parameters', collapse=False)
        self.addControl('channel', label='Channel')
        self.callCustom(self.create_gradient, self.update_gradient, 'base_channel_mode')
        self.endLayout()

        self.beginLayout('Sampling Parameters', collapse=False)
        self.addControl('positionOffset', label='Position Offset')
        self.addControl('interpolation', label='Interpolation')
        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
