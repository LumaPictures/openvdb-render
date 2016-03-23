import pymel.core as pm

class AEvdb_samplerTemplate(pm.uitypes.AETemplate):
    def __init__(self, node_name):
        self.beginScrollLayout()

        self.beginLayout('Channel Parameters', collapse=False)

        self.addControl('channel', label='Channel')
        self.addControl('positionOffset', label='Position Offset')
        self.addControl('interpolation', label='Interpolation')

        self.endLayout()

        self.addExtraControls()
        self.endScrollLayout()
