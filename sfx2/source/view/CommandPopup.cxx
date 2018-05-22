/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CommandPopup.hxx"

#include <vcl/layout.hxx>
#include <workwin.hxx>
#include <sfx2/msgpool.hxx>
#include <sfx2/bindings.hxx>

#include <comphelper/processfactory.hxx>
#include <comphelper/dispatchcommand.hxx>

#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XDispatchInformationProvider.hpp>
#include <com/sun/star/frame/theUICommandDescription.hpp>
#include <com/sun/star/ui/theUICategoryDescription.hpp>

#include <com/sun/star/ui/theModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/XUIConfigurationManagerSupplier.hpp>

#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>

#include <vcl/commandinfoprovider.hxx>

using namespace css;

struct CurrentEntry
{
    OUString m_aCommandURL;
    OUString m_aTooltip;

    CurrentEntry(OUString const& rCommandURL, OUString const& rTooltip)
        : m_aCommandURL(rCommandURL)
        , m_aTooltip(rTooltip)
    {
    }
};

struct MenuContent
{
    OUString m_aCommandURL;
    OUString m_aMenuLabel;
    OUString m_aFullLabelWithPath;
    OUString m_aTooltip;
    std::vector<MenuContent> m_aSubMenuContent;
};

#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XNotifyingDispatch.hpp>
#include <com/sun/star/util/URL.hpp>
#include <com/sun/star/util/URLTransformer.hpp>

class MenuContentHandler
{
private:
    css::uno::Reference<css::frame::XFrame> m_xFrame;
    MenuContent m_aMenuContent;
    OUString m_sModuleLongName;

public:
    MenuContentHandler(uno::Reference<frame::XFrame> const& xFrame)
        : m_xFrame(xFrame)
        , m_sModuleLongName(vcl::CommandInfoProvider::GetModuleIdentifier(xFrame))
    {
        auto xComponentContext = comphelper::getProcessComponentContext();

        uno::Reference<ui::XModuleUIConfigurationManagerSupplier> xModuleConfigSupplier;
        xModuleConfigSupplier.set(
            ui::theModuleUIConfigurationManagerSupplier::get(xComponentContext));

        uno::Reference<ui::XUIConfigurationManager> xConfigurationManager;
        xConfigurationManager = xModuleConfigSupplier->getUIConfigurationManager(m_sModuleLongName);

        uno::Reference<container::XIndexAccess> xConfigData;
        xConfigData = xConfigurationManager->getSettings("private:resource/menubar/menubar", false);

        gatherMenuContent(xConfigData, m_aMenuContent);
    }

    void gatherMenuContent(uno::Reference<container::XIndexAccess> const& xIndexAccess,
                           MenuContent& rMenuContent)
    {
        for (sal_Int32 n = 0; n < xIndexAccess->getCount(); n++)
        {
            MenuContent aNewContent;
            uno::Sequence<beans::PropertyValue> aProperties;
            uno::Reference<container::XIndexAccess> xIndexContainer;

            if (!(xIndexAccess->getByIndex(n) >>= aProperties))
                continue;

            bool bIsVisible = true;
            bool bIsEnabled = true;

            for (auto const& rProperty : aProperties)
            {
                OUString aPropertyName = rProperty.Name;
                if (aPropertyName == "CommandURL")
                    rProperty.Value >>= aNewContent.m_aCommandURL;
                else if (aPropertyName == "ItemDescriptorContainer")
                    rProperty.Value >>= xIndexContainer;
                else if (aPropertyName == "IsVisible")
                    rProperty.Value >>= bIsVisible;
                else if (aPropertyName == "Enabled")
                    rProperty.Value >>= bIsEnabled;
            }

            if (!bIsEnabled || !bIsVisible)
                continue;

            auto aCommandProperties = vcl::CommandInfoProvider::GetCommandProperties(
                aNewContent.m_aCommandURL, m_sModuleLongName);
            OUString aLabel = vcl::CommandInfoProvider::GetLabelForCommand(aCommandProperties);
            aNewContent.m_aMenuLabel = aLabel;

            if (!rMenuContent.m_aFullLabelWithPath.isEmpty())
                aNewContent.m_aFullLabelWithPath = rMenuContent.m_aFullLabelWithPath + " / ";
            aNewContent.m_aFullLabelWithPath += aNewContent.m_aMenuLabel;

            aNewContent.m_aTooltip = vcl::CommandInfoProvider::GetTooltipForCommand(
                aNewContent.m_aCommandURL, aCommandProperties, m_xFrame);

            if (xIndexContainer.is())
                gatherMenuContent(xIndexContainer, aNewContent);

            rMenuContent.m_aSubMenuContent.push_back(aNewContent);
        }
    }

    void findInMenu(OUString const& rText, std::unique_ptr<weld::TreeView>& rpCommandTreeView,
                    std::vector<CurrentEntry>& rCommandList)
    {
        findInMenuRecursive(m_aMenuContent, rText, rpCommandTreeView, rCommandList);
    }

private:
    void findInMenuRecursive(MenuContent const& rMenuContent, OUString const& rText,
                             std::unique_ptr<weld::TreeView>& rpCommandTreeView,
                             std::vector<CurrentEntry>& rCommandList)
    {
        for (MenuContent const& aSubContent : rMenuContent.m_aSubMenuContent)
        {
            if (aSubContent.m_aMenuLabel.toAsciiLowerCase().startsWith(rText))
            {
                OUString sCommandURL = aSubContent.m_aCommandURL;
                util::URL aCommandURL;
                aCommandURL.Complete = sCommandURL;
                uno::Reference<uno::XComponentContext> xContext
                    = comphelper::getProcessComponentContext();
                uno::Reference<util::XURLTransformer> xParser
                    = util::URLTransformer::create(xContext);
                xParser->parseStrict(aCommandURL);

                auto* pViewFrame = SfxViewFrame::Current();

                SfxSlotPool& rSlotPool = SfxSlotPool::GetSlotPool(pViewFrame);
                const SfxSlot* pSlot = rSlotPool.GetUnoSlot(aCommandURL.Path);
                if (pSlot)
                {
                    std::unique_ptr<SfxPoolItem> pState;
                    SfxItemState eState
                        = pViewFrame->GetBindings().QueryState(pSlot->GetSlotId(), pState);

                    if (eState != SfxItemState::DISABLED)
                    {
                        auto xGraphic = vcl::CommandInfoProvider::GetXGraphicForCommand(sCommandURL,
                                                                                        m_xFrame);
                        rCommandList.emplace_back(sCommandURL, aSubContent.m_aTooltip);

                        auto pIter = rpCommandTreeView->make_iterator();
                        rpCommandTreeView->insert(nullptr, -1, &aSubContent.m_aFullLabelWithPath,
                                                  nullptr, nullptr, nullptr, false, pIter.get());
                        rpCommandTreeView->set_image(*pIter, xGraphic);
                    }
                }
            }
            findInMenuRecursive(aSubContent, rText, rpCommandTreeView, rCommandList);
        }
    }
};

CommandListBox::CommandListBox(vcl::Window* pParent, CommandPopup& rPopUp,
                               uno::Reference<frame::XFrame> const& xFrame)
    : InterimItemWindow(pParent, "sfx/ui/commandpopup.ui", "CommandBox")
    , m_rPopUp(rPopUp)
    , m_pEntry(m_xBuilder->weld_entry("command_entry"))
    , m_pCommandTreeView(m_xBuilder->weld_tree_view("command_treeview"))
    , m_pMenuContentHandler(std::make_unique<MenuContentHandler>(xFrame))
{
    m_pEntry->connect_changed(LINK(this, CommandListBox, ModifyHdl));
    m_pEntry->connect_key_press(LINK(this, CommandListBox, TreeViewKeyPress));
    m_pCommandTreeView->connect_query_tooltip(LINK(this, CommandListBox, QueryTooltip));
    m_pCommandTreeView->connect_row_activated(LINK(this, CommandListBox, RowActivated));
}

void CommandListBox::initialize()
{
    Size aSize(400, 400);
    SetSizePixel(aSize);
    m_rPopUp.SetSizePixel(aSize);
    Size aFrameSize = m_rPopUp.GetParent()->GetOutputSizePixel();
    m_rPopUp.StartPopupMode(tools::Rectangle(Point(aFrameSize.Width(), 0), Size(0, 0)),
                            FloatWinPopupFlags::Down | FloatWinPopupFlags::GrabFocus);

    Show();
    GrabFocus();
    m_pEntry->grab_focus();
}

void CommandListBox::dispose()
{
    m_pEntry.reset();
    m_pCommandTreeView.reset();

    InterimItemWindow::dispose();
}

IMPL_LINK_NOARG(CommandListBox, QueryTooltip, const weld::TreeIter&, OUString)
{
    size_t nSelected = m_pCommandTreeView->get_selected_index();
    if (nSelected < m_aCommandList.size())
    {
        auto const& rCurrent = m_aCommandList[nSelected];
        return rCurrent.m_aTooltip;
    }
    return OUString();
}

IMPL_LINK_NOARG(CommandListBox, RowActivated, weld::TreeView&, bool)
{
    OUString aCommandURL;
    int nSelected = m_pCommandTreeView->get_selected_index();
    if (nSelected < int(m_aCommandList.size()))
    {
        auto const& rCurrent = m_aCommandList[nSelected];
        aCommandURL = rCurrent.m_aCommandURL;
    }
    dispatchCommandAndClose(aCommandURL);
    return true;
}

IMPL_LINK(CommandListBox, TreeViewKeyPress, const KeyEvent&, rKeyEvent, bool)
{
    if (rKeyEvent.GetKeyCode().GetCode() == KEY_DOWN || rKeyEvent.GetKeyCode().GetCode() == KEY_UP)
    {
        int nDirection = rKeyEvent.GetKeyCode().GetCode() == KEY_DOWN ? 1 : -1;
        int nNewIndex = m_pCommandTreeView->get_selected_index() + nDirection;
        nNewIndex = std::clamp(nNewIndex, 0, m_pCommandTreeView->n_children());
        m_pCommandTreeView->select(nNewIndex);
        m_pCommandTreeView->set_cursor(nNewIndex);
        return true;
    }
    else if (rKeyEvent.GetKeyCode().GetCode() == KEY_RETURN)
    {
        RowActivated(*m_pCommandTreeView);
    }

    return false;
}

IMPL_LINK_NOARG(CommandListBox, ModifyHdl, weld::Entry&, void)
{
    m_pCommandTreeView->clear();
    m_aCommandList.clear();

    OUString sText = m_pEntry->get_text();
    if (sText.isEmpty())
        return;

    m_pCommandTreeView->freeze();
    m_pMenuContentHandler->findInMenu(sText.toAsciiLowerCase(), m_pCommandTreeView, m_aCommandList);
    m_pCommandTreeView->thaw();

    if (m_pCommandTreeView->n_children() > 0)
    {
        m_pCommandTreeView->set_cursor(0);
        m_pCommandTreeView->select(0);
    }

    m_pEntry->grab_focus();
}

void CommandListBox::dispatchCommandAndClose(OUString const& rCommand)
{
    m_rPopUp.EndPopupMode(FloatWinPopupEndFlags::CloseAll);
    if (!rCommand.isEmpty())
        comphelper::dispatchCommand(rCommand, uno::Sequence<beans::PropertyValue>());
}

CommandPopup::CommandPopup(vcl::Window* pParent)
    : FloatingWindow(pParent, WB_BORDER | WB_SYSTEMWINDOW)
{
}

CommandPopup::~CommandPopup() { disposeOnce(); }

void CommandPopup::dispose() { FloatingWindow::dispose(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
