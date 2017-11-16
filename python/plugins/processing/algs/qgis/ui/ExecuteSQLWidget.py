# -*- coding: utf-8 -*-

"""
***************************************************************************
    ExecuteSQLWidget.py
    ---------------------
    Date                 : November 2017
    Copyright            : (C) 2017 by Paul Blottiere
    Email                : blottiere dot paul at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Paul Blottiere'
__date__ = 'November 2017'
__copyright__ = '(C) 2017, Paul Blottiere'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

import os

from qgis.PyQt import uic
from qgis.PyQt.QtWidgets import QTreeWidgetItem
from qgis.PyQt.QtCore import Qt

from qgis.core import QgsApplication
from qgis.core import QgsExpressionContextScope

from processing.gui.wrappers import WidgetWrapper

pluginPath = os.path.dirname(__file__)
WIDGET, BASE = uic.loadUiType(os.path.join(pluginPath, 'ExecuteSQLWidgetBase.ui'))


class ExecuteSQLWidget(BASE, WIDGET):

    def __init__(self):
        super(ExecuteSQLWidget, self).__init__(None)
        self.setupUi(self)

        self.mAddButton.setIcon(QgsApplication.getThemeIcon('/symbologyAdd.svg'))
        self.mRemoveButton.setIcon(QgsApplication.getThemeIcon('/symbologyRemove.svg'))
        self.mAddButton.clicked.connect(self.add)
        self.mRemoveButton.clicked.connect(self.remove)

    def add(self):
        item = QTreeWidgetItem()
        item.setText(0, 'name')
        item.setText(1, 'value')
        item.setFlags(item.flags() | Qt.ItemIsEditable)
        self.mTreeParameters.addTopLevelItem(item)

    def remove(self):
        item = self.mTreeParameters.currentItem()

        if item:
            self.mTreeParameters.invisibleRootItem().removeChild(item)

    def setValue(self, value):
        pass

    def value(self):
        scope = QgsExpressionContextScope()

        for i in range(self.mTreeParameters.topLevelItemCount()):
            item = self.mTreeParameters.topLevelItem(i)
            if not item:
                continue

            scope.setVariable(item.text(0), item.text(1), True)

        return scope


class ExecuteSQLWidgetWrapper(WidgetWrapper):

    def createWidget(self):
        return ExecuteSQLWidget()

    def setValue(self, value):
        self.widget.setValue(value)

    def value(self):
        return self.widget.value()
