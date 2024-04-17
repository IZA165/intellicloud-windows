//
//  ShareViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import AppKit
import FileProvider
import OSLog
import QuickLookThumbnailing

class ShareViewController: NSViewController {
    let itemIdentifiers: [NSFileProviderItemIdentifier]

    @IBOutlet weak var fileNameIcon: NSImageView!
    @IBOutlet weak var fileNameLabel: NSTextField!
    @IBOutlet weak var descriptionLabel: NSTextField!
    @IBOutlet weak var closeButton: NSButton!
    @IBOutlet weak var tableView: NSTableView!

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier]) {
        self.itemIdentifiers = itemIdentifiers
        super.init(nibName: nil, bundle: nil)

        guard let firstItem = itemIdentifiers.first else {
            Logger.shareViewController.error("called without items")
            closeAction(self)
            return
        }

        Task {
            await processItemIdentifier(firstItem)
        }
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    @IBAction func closeAction(_ sender: Any) {
        actionViewController.extensionContext.completeRequest()
    }

    private func processItemIdentifier(_ itemIdentifier: NSFileProviderItemIdentifier) async {
        guard let manager = NSFileProviderManager(for: actionViewController.domain) else {
            fatalError("NSFileProviderManager isn't expected to fail")
        }

        do {
            let itemUrl = try await manager.getUserVisibleURL(for: itemIdentifier)
            await updateDisplay(itemUrl: itemUrl)
        } catch let error {
            let errorString = "Error processing item: \(error)"
            Logger.shareViewController.error("\(errorString)")
            fileNameLabel.stringValue = "Unknown item"
            descriptionLabel.stringValue = errorString
        }
    }

    private func updateDisplay(itemUrl: URL) async {
        fileNameLabel.stringValue = itemUrl.lastPathComponent

        let request = QLThumbnailGenerator.Request(
            fileAt: itemUrl,
            size: CGSize(width: 128, height: 128),
            scale: 1.0,
            representationTypes: .icon
        )

        let generator = QLThumbnailGenerator.shared
        let fileThumbnail = await withCheckedContinuation { continuation in
            generator.generateRepresentations(for: request) { thumbnail, type, error in
                if thumbnail == nil || error != nil {
                    Logger.shareViewController.error("Could not get thumbnail: \(error)")
                }
                continuation.resume(returning: thumbnail)
            }
        }
        fileNameIcon.image = fileThumbnail?.nsImage
    }
}
