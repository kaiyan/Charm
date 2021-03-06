/*
  SmartNameCache.h

  This file is part of Charm, a task-based time tracking application.

  Copyright (C) 2012-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

  Author: Frank Osterfeld <frank.osterfeld@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SMARTNAMECACHE_H
#define SMARTNAMECACHE_H

#include "Task.h"

class SmartNameCache
{
public:
    void setAllTasks(const TaskList &taskList);
    QString smartName(const TaskId &id) const;
    void addTask(const Task &task);
    void modifyTask(const Task &task);
    void deleteTask(const Task &task);
    void clearTasks();

private:
    void regenerateSmartNames();
    void sortTasks();
    Task findTask(TaskId id) const;
    QString makeCombined(const Task &task) const;

private:
    QMap<TaskId, QString> m_smartTaskNamesById;
    TaskList m_tasks;
};

#endif
