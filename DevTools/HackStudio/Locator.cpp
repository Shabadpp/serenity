/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Locator.h"
#include "Project.h"
#include <LibGUI/GBoxLayout.h>
#include <LibGUI/GTableView.h>
#include <LibGUI/GTextBox.h>
#include <LibGUI/GWindow.h>

extern RefPtr<Project> g_project;
extern void open_file(const String&);
static RefPtr<GraphicsBitmap> s_file_icon;
static RefPtr<GraphicsBitmap> s_cplusplus_icon;
static RefPtr<GraphicsBitmap> s_header_icon;

class LocatorSuggestionModel final : public GModel {
public:
    explicit LocatorSuggestionModel(Vector<String>&& suggestions)
        : m_suggestions(move(suggestions))
    {
    }

    enum Column {
        Icon,
        Name,
        __Column_Count,
    };
    virtual int row_count(const GModelIndex& = GModelIndex()) const override { return m_suggestions.size(); }
    virtual int column_count(const GModelIndex& = GModelIndex()) const override { return Column::__Column_Count; }
    virtual GVariant data(const GModelIndex& index, Role role = Role::Display) const override
    {
        auto& suggestion = m_suggestions.at(index.row());
        if (role == Role::Display) {
            if (index.column() == Column::Name)
                return suggestion;
            if (index.column() == Column::Icon) {
                if (suggestion.ends_with(".cpp"))
                    return *s_cplusplus_icon;
                if (suggestion.ends_with(".h"))
                    return *s_header_icon;
                return *s_file_icon;
            }
        }
        return {};
    }
    virtual void update() override {};

private:
    Vector<String> m_suggestions;
};

class LocatorTextBox final : public GTextBox {
    C_OBJECT(LocatorTextBox)
public:
    virtual ~LocatorTextBox() override {}

    Function<void()> on_up;
    Function<void()> on_down;

    virtual void keydown_event(GKeyEvent& event) override
    {
        if (event.key() == Key_Up)
            on_up();
        else if (event.key() == Key_Down)
            on_down();

        GTextBox::keydown_event(event);
    }

private:
    LocatorTextBox(GWidget* parent)
        : GTextBox(parent)
    {
    }
};

Locator::Locator(GWidget* parent)
    : GWidget(parent)
{
    if (!s_cplusplus_icon) {
        s_file_icon = GraphicsBitmap::load_from_file("/res/icons/16x16/filetype-unknown.png");
        s_cplusplus_icon = GraphicsBitmap::load_from_file("/res/icons/16x16/filetype-cplusplus.png");
        s_header_icon = GraphicsBitmap::load_from_file("/res/icons/16x16/filetype-header.png");
    }

    set_layout(make<GBoxLayout>(Orientation::Vertical));
    set_size_policy(SizePolicy::Fill, SizePolicy::Fixed);
    set_preferred_size(0, 20);
    m_textbox = LocatorTextBox::construct(this);
    m_textbox->on_change = [this] {
        update_suggestions();
    };
    m_textbox->on_escape_pressed = [this] {
        m_popup_window->hide();
    };
    m_textbox->on_up = [this] {
        GModelIndex new_index = m_suggestion_view->selection().first();
        if (new_index.is_valid())
            new_index = m_suggestion_view->model()->index(new_index.row() - 1);
        else
            new_index = m_suggestion_view->model()->index(0);

        if (new_index.is_valid()) {
            m_suggestion_view->selection().set(new_index);
            m_suggestion_view->scroll_into_view(new_index, Orientation::Vertical);
        }
    };
    m_textbox->on_down = [this] {
        GModelIndex new_index = m_suggestion_view->selection().first();
        if (new_index.is_valid())
            new_index = m_suggestion_view->model()->index(new_index.row() + 1);
        else
            new_index = m_suggestion_view->model()->index(0);

        if (new_index.is_valid()) {
            m_suggestion_view->selection().set(new_index);
            m_suggestion_view->scroll_into_view(new_index, Orientation::Vertical);
        }
    };

    m_textbox->on_return_pressed = [this] {
        auto selected_index = m_suggestion_view->selection().first();
        if (!selected_index.is_valid())
            return;
        open_suggestion(selected_index);
    };

    m_popup_window = GWindow::construct();
    // FIXME: This is obviously not a tooltip window, but it's the closest thing to what we want atm.
    m_popup_window->set_window_type(GWindowType::Tooltip);
    m_popup_window->set_rect(0, 0, 500, 200);

    m_suggestion_view = GTableView::construct(nullptr);
    m_suggestion_view->set_size_columns_to_fit_content(true);
    m_suggestion_view->set_headers_visible(false);
    m_popup_window->set_main_widget(m_suggestion_view);

    m_suggestion_view->on_activation = [this](auto& index) {
        open_suggestion(index);
    };
}

Locator::~Locator()
{
}

void Locator::open_suggestion(const GModelIndex& index)
{
    auto filename_index = m_suggestion_view->model()->index(index.row(), LocatorSuggestionModel::Column::Name);
    auto filename = m_suggestion_view->model()->data(filename_index, GModel::Role::Display).to_string();
    open_file(filename);
    close();
}

void Locator::open()
{
    m_textbox->set_focus(true);
    if (!m_textbox->text().is_empty()) {
        m_textbox->select_all();
        m_popup_window->show();
    }
}

void Locator::close()
{
    m_popup_window->hide();
}

void Locator::update_suggestions()
{
    auto typed_text = m_textbox->text();
    Vector<String> suggestions;
    g_project->for_each_text_file([&](auto& file) {
        if (file.name().contains(typed_text))
            suggestions.append(file.name());
    });
    dbg() << "I have " << suggestions.size() << " suggestion(s):";
    for (auto& s : suggestions) {
        dbg() << "    " << s;
    }

    m_suggestion_view->set_model(adopt(*new LocatorSuggestionModel(move(suggestions))));

    m_popup_window->move_to(screen_relative_rect().top_left().translated(0, -m_popup_window->height()));
    dbg() << "Popup rect: " << m_popup_window->rect();
    m_popup_window->show();
}
