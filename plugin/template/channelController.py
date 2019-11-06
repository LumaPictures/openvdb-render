# Copyright 2019 Luma Pictures
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import pymel.core as pm

class channelController:
    def create_gradient_params(self, channel, node_name):
        self.addControl('%sChannelMode' % channel, label='Channel Mode')

        self.beginLayout('Channel Controls', collapse=True)

        self.addControl('%sInputMin' % channel, label='Input Min')
        self.addControl('%sInputMax' % channel, label='Input Max')
        pm.mel.eval('source AEaddRampControl.mel; AEaddRampControl("%s.%sFloatRamp")' % (node_name, channel))
        self.addControl('%sContrast' % channel, label='Contrast')
        self.addControl('%sContrastPivot' % channel, label='Contrast Pivot')
        self.addControl('%sBias' % channel, label='Bias')
        self.addControl('%sGain' % channel, label='Gain')
        self.addControl('%sOutputMin' % channel, label='Output Min')
        self.addControl('%sOutputMax' % channel, label='Output Max')
        self.addControl('%sClampMin' % channel, label='Clamp Min')
        self.addControl('%sClampMax' % channel, label='Clamp Max')

        self.addSeparator()
        self.addControl('%sInputMin' % channel, label='Input Min')
        self.addControl('%sInputMax' % channel, label='Input Max')
        pm.mel.eval('source AEaddRampControl.mel; AEaddRampControl("%s.%sRgbRamp")' % (node_name, channel))
        self.addControl('%sGamma' % channel, label='Gamma')
        self.addControl('%sHueShift' % channel, label='Hue Shift')
        self.addControl('%sSaturation' % channel, label='Saturation')
        self.addControl('%sContrast' % channel, label='Contrast')
        self.addControl('%sContrastPivot' % channel, label='Contrast Pivot')
        self.addControl('%sExposure' % channel, label='Exposure')
        self.addControl('%sMultiply' % channel, label='Multiply')
        self.addControl('%sAdd' % channel, label='Add')

        self.endLayout()
