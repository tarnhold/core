/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/layout.hxx>

#include <sfx2/dllapi.h>
#include <sfx2/viewfrm.hxx>
#include <vcl/floatwin.hxx>

#include <com/sun/star/container/XNameAccess.hpp>

#include <vcl/InterimItemWindow.hxx>
#include <vcl/weld.hxx>
#include <vcl/window.hxx>

struct CurrentEntry;
class MenuContentHandler;

class SFX2_DLLPUBLIC CommandPopup : public FloatingWindow
{
public:
    explicit CommandPopup(vcl::Window* pWorkWindow);

    ~CommandPopup() override;

    void dispose() override;
};

class SFX2_DLLPUBLIC CommandListBox final : public InterimItemWindow
{
private:
    CommandPopup& m_rPopUp;

    std::unique_ptr<weld::Entry> m_pEntry;
    std::unique_ptr<weld::TreeView> m_pCommandTreeView;

    std::vector<CurrentEntry> m_aCommandList;
    OUString m_PreviousText;
    std::unique_ptr<MenuContentHandler> m_pMenuContentHandler;

    DECL_LINK(QueryTooltip, const weld::TreeIter&, OUString);
    DECL_LINK(RowActivated, weld::TreeView&, bool);
    DECL_LINK(ModifyHdl, weld::Entry&, void);
    DECL_LINK(SelectionChanged, weld::TreeView&, void);
    DECL_LINK(TreeViewKeyPress, const KeyEvent&, bool);

    void dispatchCommandAndClose(OUString const& rCommand);

public:
    CommandListBox(vcl::Window* pParent, CommandPopup& rPopUp,
                   css::uno::Reference<css::frame::XFrame> const& xFrame);

    void initialize();

    void dispose() override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
